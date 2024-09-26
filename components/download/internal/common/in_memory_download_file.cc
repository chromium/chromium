// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/download/internal/common/in_memory_download_file.h"

#include "base/android/jni_string.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/process/process_handle.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "components/download/public/common/download_destination_observer.h"
#include "crypto/secure_hash.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/download/internal/common/jni_headers/InMemoryDownloadFile_jni.h"

namespace download {
namespace {
const char kDefaultFileName[] = "download";

void OnRenameComplete(const base::FilePath& file_path,
                      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                      DownloadFile::RenameCompletionCallback callback) {
  task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                DOWNLOAD_INTERRUPT_REASON_NONE, file_path));
}
}  // namespace

InMemoryDownloadFile::InMemoryDownloadFile(
    std::unique_ptr<DownloadSaveInfo> save_info,
    std::unique_ptr<InputStream> stream,
    base::WeakPtr<DownloadDestinationObserver> observer)
    : save_info_(std::move(save_info)),
      input_stream_(std::move(stream)),
      main_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      observer_(observer) {}

InMemoryDownloadFile::~InMemoryDownloadFile() {
  Cancel();
}

void InMemoryDownloadFile::Initialize(
    InitializeCallback initialize_callback,
    CancelRequestCallback cancel_request_callback,
    const DownloadItem::ReceivedSlices& received_slices) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  base::FilePath filename = save_info_->file_path.BaseName();

  java_ref_ = Java_InMemoryDownloadFile_createFile(
      env, filename.empty() ? kDefaultFileName : filename.value());
  if (!java_ref_) {
    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(initialize_callback),
                                  DOWNLOAD_INTERRUPT_REASON_FILE_FAILED,
                                  /*bytes_wasted=*/0));
    return;
  }

  int fd = Java_InMemoryDownloadFile_getFd(env, java_ref_);
  memory_file_path_ = base::FilePath(base::StringPrintf(
      "/proc/%d/fd/%d", static_cast<int>(base::GetCurrentProcId()), fd));

  SendUpdate();

  input_stream_->Initialize();

  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(initialize_callback),
                     DOWNLOAD_INTERRUPT_REASON_NONE, /*bytes_wasted=*/0));

  StreamActive(MOJO_RESULT_OK);
}

void InMemoryDownloadFile::AddInputStream(std::unique_ptr<InputStream> stream,
                                          int64_t offset) {
  DCHECK(false) << "In memory file shouldn't have more than 1 input stream";
}

void InMemoryDownloadFile::RenameAndUniquify(
    const base::FilePath& full_path,
    RenameCompletionCallback callback) {
  OnRenameComplete(memory_file_path_, main_task_runner_, std::move(callback));
}

void InMemoryDownloadFile::RenameAndAnnotate(
    const base::FilePath& full_path,
    const std::string& client_guid,
    const GURL& source_url,
    const GURL& referrer_url,
    const std::optional<url::Origin>& request_initiator,
    mojo::PendingRemote<quarantine::mojom::Quarantine> remote_quarantine,
    RenameCompletionCallback callback) {
  OnRenameComplete(memory_file_path_, main_task_runner_, std::move(callback));
}

void InMemoryDownloadFile::Detach() {}

void InMemoryDownloadFile::Cancel() {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  Java_InMemoryDownloadFile_destroy(env, java_ref_);
}

void InMemoryDownloadFile::SetPotentialFileLength(int64_t length) {}

const base::FilePath& InMemoryDownloadFile::FullPath() const {
  return memory_file_path_;
}

bool InMemoryDownloadFile::InProgress() const {
  return true;
}

void InMemoryDownloadFile::Pause() {}

void InMemoryDownloadFile::Resume() {}

void InMemoryDownloadFile::PublishDownload(RenameCompletionCallback callback) {
  // This shouldn't get called.
  DCHECK(false);
}

void InMemoryDownloadFile::StreamActive(MojoResult result) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  scoped_refptr<net::IOBuffer> incoming_data;
  size_t incoming_data_size = 0;
  InputStream::StreamState state(InputStream::EMPTY);
  DownloadInterruptReason reason = DOWNLOAD_INTERRUPT_REASON_NONE;
  // Take care of any file local activity required.
  do {
    state = input_stream_->Read(&incoming_data, &incoming_data_size);
    switch (state) {
      case InputStream::EMPTY:
        break;
      case InputStream::HAS_DATA:
        Java_InMemoryDownloadFile_writeData(
            env, java_ref_,
            std::vector<unsigned char>(
                incoming_data->data(),
                incoming_data->data() + incoming_data_size));
        total_bytes_ += incoming_data_size;
        rate_estimator_.Increment(incoming_data_size);
        break;
      case InputStream::WAIT_FOR_COMPLETION:
        input_stream_->RegisterCompletionCallback(
            base::BindOnce(&InMemoryDownloadFile::OnStreamCompleted,
                           weak_factory_.GetWeakPtr()));
        break;
      case InputStream::COMPLETE:
        break;
    }
  } while (state == InputStream::HAS_DATA &&
           reason == DOWNLOAD_INTERRUPT_REASON_NONE);

  if (state == InputStream::EMPTY) {
    input_stream_->RegisterDataReadyCallback(base::BindRepeating(
        &InMemoryDownloadFile::StreamActive, weak_factory_.GetWeakPtr()));
  }

  if (state == InputStream::COMPLETE) {
    OnStreamCompleted();
  } else if (reason != DOWNLOAD_INTERRUPT_REASON_NONE) {
    NotifyObserver(reason, state);
  }
}

void InMemoryDownloadFile::SendUpdate() {
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DownloadDestinationObserver::DestinationUpdate, observer_,
                     total_bytes_, rate_estimator_.GetCountPerSecond(),
                     std::vector<DownloadItem::ReceivedSlice>()));
}

void InMemoryDownloadFile::OnStreamCompleted() {
  SendUpdate();
  input_stream_->ClearDataReadyCallback();
  weak_factory_.InvalidateWeakPtrs();

  JNIEnv* env = jni_zero::AttachCurrentThread();
  Java_InMemoryDownloadFile_finish(env, java_ref_);
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DownloadDestinationObserver::DestinationCompleted,
                     observer_, total_bytes_,
                     crypto::SecureHash::Create(crypto::SecureHash::SHA256)));
}

void InMemoryDownloadFile::NotifyObserver(
    DownloadInterruptReason reason,
    InputStream::StreamState stream_state) {
  input_stream_->ClearDataReadyCallback();
  SendUpdate();  // Make info up to date before error.
  Cancel();
  // Error case for both upstream source and file write.
  // Shut down processing and signal an error to our observer.
  // Our observer will clean us up.
  weak_factory_.InvalidateWeakPtrs();
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DownloadDestinationObserver::DestinationError, observer_,
                     reason, total_bytes_,
                     crypto::SecureHash::Create(crypto::SecureHash::SHA256)));
}

bool InMemoryDownloadFile::IsMemoryFile() {
  return true;
}

}  //  namespace download
