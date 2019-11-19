// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/model_loader.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/bookmarks/browser/bookmark_storage.h"
#include "components/bookmarks/browser/url_index.h"

namespace bookmarks {

namespace {

// TODO(mastiz): Remove this kill switch asap since the UMA metrics entail
// negligible risks for stability or performance overhead.
const base::Feature kEmitExperimentalBookmarkLoadUma{
    "EmitExperimentalBookmarkLoadUma", base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace

// static
scoped_refptr<ModelLoader> ModelLoader::Create(
    const base::FilePath& profile_path,
    base::SequencedTaskRunner* load_sequenced_task_runner,
    std::unique_ptr<BookmarkLoadDetails> details,
    LoadCallback callback) {
  // Note: base::MakeRefCounted is not available here, as ModelLoader's
  // constructor is private.
  auto model_loader = base::WrapRefCounted(new ModelLoader());
  // We plumb the value for kEmitExperimentalBookmarkLoadUma as retrieved on
  // the UI thread to avoid issues with TSAN bots (in case there are tests that
  // override feature toggles -not necessarily this one- while bookmark loading
  // is ongoing, which is problematic due to how feature overriding for tests is
  // implemented).
  load_sequenced_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ModelLoader::DoLoadOnBackgroundThread, model_loader, profile_path,
          base::FeatureList::IsEnabled(kEmitExperimentalBookmarkLoadUma),
          base::ThreadTaskRunnerHandle::Get(), std::move(details),
          std::move(callback)));
  return model_loader;
}

void ModelLoader::BlockTillLoaded() {
  loaded_signal_.Wait();
}

ModelLoader::ModelLoader()
    : loaded_signal_(base::WaitableEvent::ResetPolicy::MANUAL,
                     base::WaitableEvent::InitialState::NOT_SIGNALED) {}

ModelLoader::~ModelLoader() = default;

void ModelLoader::DoLoadOnBackgroundThread(
    const base::FilePath& profile_path,
    bool emit_experimental_uma,
    scoped_refptr<base::SequencedTaskRunner> main_sequenced_task_runner,
    std::unique_ptr<BookmarkLoadDetails> details,
    LoadCallback callback) {
  LoadBookmarks(profile_path, emit_experimental_uma, details.get());
  history_bookmark_model_ = details->url_index();
  loaded_signal_.Signal();
  main_sequenced_task_runner->PostTask(
      FROM_HERE, base::BindOnce(&ModelLoader::OnFinishedLoad, this,
                                std::move(details), std::move(callback)));
}

void ModelLoader::OnFinishedLoad(std::unique_ptr<BookmarkLoadDetails> details,
                                 LoadCallback callback) {
  std::move(callback).Run(std::move(details));
}

}  // namespace bookmarks
