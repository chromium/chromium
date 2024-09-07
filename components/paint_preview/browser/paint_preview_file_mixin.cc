// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/browser/paint_preview_file_mixin.h"

#include <string_view>
#include <utility>

#include "base/files/file_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "ui/accessibility/mojom/ax_tree_update.mojom.h"

namespace paint_preview {

namespace {

const char kPaintPreviewDir[] = "paint_preview";
const char kAxTreeFilename[] = "ax_tree.message";

// Threadpool task to read the proto from disk.
std::pair<PaintPreviewFileMixin::ProtoReadStatus,
          std::unique_ptr<PaintPreviewProto>>
GetProto(scoped_refptr<FileManager> file_manager,
         const DirectoryKey& key,
         std::optional<base::TimeDelta> expiry_horizon) {
  TRACE_EVENT0("paint_preview", "PaintPreviewFileMixin::GetProto");
  if (expiry_horizon.has_value()) {
    auto file_info = file_manager->GetInfo(key);
    if (!file_info.has_value())
      return std::make_pair(PaintPreviewFileMixin::ProtoReadStatus::kNoProto,
                            nullptr);

    if (file_info->last_modified + expiry_horizon.value() <
        base::Time::NowFromSystemTime()) {
      return std::make_pair(PaintPreviewFileMixin::ProtoReadStatus::kExpired,
                            nullptr);
    }
  }
  auto result = file_manager->DeserializePaintPreviewProto(key);
  PaintPreviewFileMixin::ProtoReadStatus status =
      PaintPreviewFileMixin::ProtoReadStatus::kNoProto;
  switch (result.first) {
    case FileManager::ProtoReadStatus::kOk:
      status = PaintPreviewFileMixin::ProtoReadStatus::kOk;
      break;
    case FileManager::ProtoReadStatus::kNoProto:
      status = PaintPreviewFileMixin::ProtoReadStatus::kNoProto;
      break;
    case FileManager::ProtoReadStatus::kDeserializationError:
      status = PaintPreviewFileMixin::ProtoReadStatus::kDeserializationError;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return std::make_pair(status, std::move(result.second));
}

// Callback executed on threadpool read complete.
void OnReadProto(PaintPreviewFileMixin::OnReadProtoCallback callback,
                 std::pair<PaintPreviewFileMixin::ProtoReadStatus,
                           std::unique_ptr<PaintPreviewProto>> result) {
  TRACE_EVENT0("paint_preview", "PaintPreviewFileMixin::OnReadProto");
  std::move(callback).Run(result.first, std::move(result.second));
}

}  // namespace

PaintPreviewFileMixin::PaintPreviewFileMixin(
    const base::FilePath& path,
    std::string_view ascii_feature_name)
    : task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN,
           base::ThreadPolicy::MUST_USE_FOREGROUND})),
      file_manager_(base::MakeRefCounted<FileManager>(
          path.AppendASCII(kPaintPreviewDir).AppendASCII(ascii_feature_name),
          task_runner_)) {}

PaintPreviewFileMixin::~PaintPreviewFileMixin() = default;

void PaintPreviewFileMixin::GetCapturedPaintPreviewProto(
    const DirectoryKey& key,
    std::optional<base::TimeDelta> expiry_horizon,
    OnReadProtoCallback on_read_proto_callback) {
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&GetProto, file_manager_, key, expiry_horizon),
      base::BindOnce(OnReadProto, std::move(on_read_proto_callback)));
}

void PaintPreviewFileMixin::WriteAXTreeUpdate(
    const DirectoryKey& key,
    base::OnceCallback<void(bool)> finished_callback,
    ui::AXTreeUpdate& ax_tree_update) {
  std::vector<uint8_t> ax_data =
      ax::mojom::AXTreeUpdate::Serialize(&ax_tree_update);
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<FileManager> file_manager, const DirectoryKey& key,
             const std::vector<uint8_t>& ax_data) {
            TRACE_EVENT0("paint_preview",
                         "PaintPreviewFileMixin::WriteAXTreeUpdate Task");
            auto directory = file_manager->CreateOrGetDirectory(key, false);
            if (!directory.has_value()) {
              return false;
            }
            return base::WriteFile(directory->AppendASCII(kAxTreeFilename),
                                   ax_data);
          },
          file_manager_, key, ax_data),
      std::move(finished_callback));
}

void PaintPreviewFileMixin::GetAXTreeUpdate(const DirectoryKey& key,
                                            OnReadAXTree callback) {
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<FileManager> file_manager,
             const DirectoryKey& key) -> std::unique_ptr<ui::AXTreeUpdate> {
            TRACE_EVENT0("paint_preview",
                         "PaintPreviewFileMixin::GetAXTreeUpdate Task");
            auto dir = file_manager->CreateOrGetDirectory(key, false);
            if (!dir.has_value()) {
              return nullptr;
            }

            auto path = dir->AppendASCII(kAxTreeFilename);
            std::string content;
            if (!base::ReadFileToString(path, &content)) {
              return nullptr;
            }

            auto update = std::make_unique<ui::AXTreeUpdate>();
            if (!ax::mojom::AXTreeUpdate::Deserialize(
                    content.data(), content.size(), update.get())) {
              return nullptr;
            }
            return update;
          },
          file_manager_, key),
      std::move(callback));
}

}  // namespace paint_preview
