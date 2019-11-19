// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/engines/broker/cleaner_engine_requests_impl.h"

#include <memory>
#include <utility>

#include "base/strings/string16.h"
#include "base/task/post_task.h"
#include "chrome/chrome_cleaner/engines/broker/cleaner_sandbox_interface.h"
#include "chrome/chrome_cleaner/engines/common/engine_digest_verifier.h"
#include "chrome/chrome_cleaner/os/digest_verifier.h"
#include "chrome/chrome_cleaner/os/file_path_sanitization.h"
#include "chrome/chrome_cleaner/os/file_remover.h"
#include "chrome/chrome_cleaner/os/layered_service_provider_wrapper.h"
#include "chrome/chrome_cleaner/pup_data/pup_data.h"

namespace chrome_cleaner {

std::unique_ptr<chrome_cleaner::FileRemoverAPI>
CreateFileRemoverWithDigestVerifier(
    std::unique_ptr<ZipArchiver> archiver,
    const base::RepeatingClosure& reboot_needed_callback) {
  auto digest = GetDigestVerifier();
  auto lsp = chrome_cleaner::LayeredServiceProviderWrapper();
  return std::make_unique<chrome_cleaner::FileRemover>(
      digest, std::move(archiver), lsp, reboot_needed_callback);
}

CleanerEngineRequestsImpl::CleanerEngineRequestsImpl(
    scoped_refptr<MojoTaskRunner> mojo_task_runner,
    InterfaceMetadataObserver* metadata_observer,
    std::unique_ptr<chrome_cleaner::FileRemoverAPI> file_remover)
    : mojo_task_runner_(mojo_task_runner),
      metadata_observer_(metadata_observer),
      file_remover_(std::move(file_remover)) {}

CleanerEngineRequestsImpl::~CleanerEngineRequestsImpl() = default;

void CleanerEngineRequestsImpl::Bind(
    mojo::PendingAssociatedRemote<mojom::CleanerEngineRequests>* remote) {
  receiver_.Bind(remote->InitWithNewEndpointAndPassReceiver());
  // There's no need to call set_disconnect_handler on this since it's an
  // associated interface. Any errors will be handled on the main EngineCommands
  // interface.
}

void CleanerEngineRequestsImpl::SandboxDeleteFile(
    const base::FilePath& file_name,
    SandboxDeleteFileCallback result_callback) {
  if (metadata_observer_)
    metadata_observer_->ObserveCall(CURRENT_FILE_AND_METHOD);

  file_remover_->RemoveNow(base::FilePath(file_name),
                           std::move(result_callback));
}

void CleanerEngineRequestsImpl::SandboxDeleteFilePostReboot(
    const base::FilePath& file_name,
    SandboxDeleteFilePostRebootCallback result_callback) {
  if (metadata_observer_)
    metadata_observer_->ObserveCall(CURRENT_FILE_AND_METHOD);

  file_remover_->RegisterPostRebootRemoval(base::FilePath(file_name),
                                           std::move(result_callback));
}

void CleanerEngineRequestsImpl::SandboxNtDeleteRegistryKey(
    const String16EmbeddedNulls& key,
    SandboxNtDeleteRegistryKeyCallback result_callback) {
  base::PostTaskAndReplyWithResult(
      FROM_HERE, {base::ThreadPool(), base::MayBlock()},
      base::BindOnce(&CleanerEngineRequestsImpl::NtDeleteRegistryKey,
                     base::Unretained(this), key),
      std::move(result_callback));
}

bool CleanerEngineRequestsImpl::NtDeleteRegistryKey(
    const String16EmbeddedNulls& key) {
  if (metadata_observer_)
    metadata_observer_->ObserveCall(CURRENT_FILE_AND_METHOD);
  return chrome_cleaner_sandbox::SandboxNtDeleteRegistryKey(key);
}

void CleanerEngineRequestsImpl::SandboxNtDeleteRegistryValue(
    const String16EmbeddedNulls& key,
    const String16EmbeddedNulls& value_name,
    SandboxNtDeleteRegistryValueCallback result_callback) {
  base::PostTaskAndReplyWithResult(
      FROM_HERE, {base::ThreadPool(), base::MayBlock()},
      base::BindOnce(&CleanerEngineRequestsImpl::NtDeleteRegistryValue,
                     base::Unretained(this), key, value_name),
      std::move(result_callback));
}

bool CleanerEngineRequestsImpl::NtDeleteRegistryValue(
    const String16EmbeddedNulls& key,
    const String16EmbeddedNulls& value_name) {
  if (metadata_observer_)
    metadata_observer_->ObserveCall(CURRENT_FILE_AND_METHOD);
  return chrome_cleaner_sandbox::SandboxNtDeleteRegistryValue(key, value_name);
}

void CleanerEngineRequestsImpl::SandboxNtChangeRegistryValue(
    const String16EmbeddedNulls& key,
    const String16EmbeddedNulls& value_name,
    const String16EmbeddedNulls& new_value,
    SandboxNtChangeRegistryValueCallback result_callback) {
  base::PostTaskAndReplyWithResult(
      FROM_HERE, {base::ThreadPool(), base::MayBlock()},
      base::BindOnce(&CleanerEngineRequestsImpl::NtChangeRegistryValue,
                     base::Unretained(this), key, value_name, new_value),
      std::move(result_callback));
}

bool CleanerEngineRequestsImpl::NtChangeRegistryValue(
    const String16EmbeddedNulls& key,
    const String16EmbeddedNulls& value_name,
    const String16EmbeddedNulls& new_value) {
  if (metadata_observer_)
    metadata_observer_->ObserveCall(CURRENT_FILE_AND_METHOD);
  return chrome_cleaner_sandbox::SandboxNtChangeRegistryValue(
      key, value_name, new_value,
      base::BindRepeating(
          &chrome_cleaner_sandbox::DefaultShouldValueBeNormalized));
}

void CleanerEngineRequestsImpl::SandboxDeleteService(
    const base::string16& name,
    SandboxDeleteServiceCallback result_callback) {
  base::PostTaskAndReplyWithResult(
      FROM_HERE, {base::ThreadPool(), base::MayBlock()},
      base::BindOnce(&CleanerEngineRequestsImpl::DeleteService,
                     base::Unretained(this), name),
      std::move(result_callback));
}

bool CleanerEngineRequestsImpl::DeleteService(const base::string16& name) {
  if (metadata_observer_)
    metadata_observer_->ObserveCall(CURRENT_FILE_AND_METHOD);
  return chrome_cleaner_sandbox::SandboxDeleteService(name);
}

void CleanerEngineRequestsImpl::SandboxDeleteTask(
    const base::string16& name,
    SandboxDeleteServiceCallback result_callback) {
  base::PostTaskAndReplyWithResult(
      FROM_HERE, {base::ThreadPool(), base::MayBlock()},
      base::BindOnce(&CleanerEngineRequestsImpl::DeleteTask,
                     base::Unretained(this), name),
      std::move(result_callback));
}

bool CleanerEngineRequestsImpl::DeleteTask(const base::string16& name) {
  if (metadata_observer_)
    metadata_observer_->ObserveCall(CURRENT_FILE_AND_METHOD);
  return chrome_cleaner_sandbox::SandboxDeleteTask(name);
}

void CleanerEngineRequestsImpl::SandboxTerminateProcess(
    uint32_t process_id,
    SandboxTerminateProcessCallback result_callback) {
  base::PostTaskAndReplyWithResult(
      FROM_HERE, {base::ThreadPool(), base::MayBlock()},
      base::BindOnce(&CleanerEngineRequestsImpl::TerminateProcess,
                     base::Unretained(this), process_id),
      std::move(result_callback));
}

bool CleanerEngineRequestsImpl::TerminateProcess(uint32_t process_id) {
  using chrome_cleaner_sandbox::TerminateProcessResult;

  if (metadata_observer_)
    metadata_observer_->ObserveCall(CURRENT_FILE_AND_METHOD);

  switch (chrome_cleaner_sandbox::SandboxTerminateProcess(process_id)) {
    case TerminateProcessResult::kSuccess:
      return true;
    case TerminateProcessResult::kFailed:
      return false;
    case TerminateProcessResult::kDenied:
      // Report that the termination was successful, because when we deny the
      // termination because of a policy and not a technical failure, we don't
      // want the caller to escalate to more aggressive methods.
      return true;
  }
}

}  // namespace chrome_cleaner
