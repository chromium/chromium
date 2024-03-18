// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_internals_utils.h"

#include <string_view>

#include "base/files/file_path.h"
#include "base/json/json_file_value_serializer.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/web_applications/file_utils_wrapper.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "content/public/browser/browser_thread.h"

namespace web_app {

namespace {

struct ErrorLogData {
  Result result;
  base::Value error_log;
};

constexpr base::TaskTraits kTaskTraits = {
    base::MayBlock(), base::TaskPriority::USER_VISIBLE,
    base::TaskShutdownBehavior::BLOCK_SHUTDOWN};

base::FilePath GetErrorLogDirectory(const base::FilePath& web_apps_directory) {
  return web_apps_directory.AppendASCII("Logs");
}

base::FilePath GetErrorLogFileName(const base::FilePath& web_apps_directory,
                                   std::string_view subsystem_name) {
  return GetErrorLogDirectory(web_apps_directory)
      .AppendASCII(subsystem_name.data())
      .AddExtensionASCII("log");
}

ErrorLogData ReadErrorLogBlocking(scoped_refptr<FileUtilsWrapper> utils,
                                  const base::FilePath& web_apps_directory,
                                  std::string_view subsystem_name) {
  base::FilePath log_file_name =
      GetErrorLogFileName(web_apps_directory, subsystem_name);

  ErrorLogData data;

  JSONFileValueDeserializer deserializer(log_file_name);
  std::string error_msg;
  std::unique_ptr<base::Value> error_log =
      deserializer.Deserialize(nullptr, &error_msg);

  if (error_log) {
    data.result = Result::kOk;
    data.error_log = std::move(*error_log);
  } else {
    data.result = Result::kError;
  }

  return data;
}

void OnReadErrorLogBlocking(ReadErrorLogCallback callback, ErrorLogData data) {
  std::move(callback).Run(data.result, std::move(data.error_log));
}

Result WriteErrorLogBlocking(scoped_refptr<FileUtilsWrapper> utils,
                             const base::FilePath& web_apps_directory,
                             std::string_view subsystem_name,
                             base::Value error_log) {
  if (!utils->CreateDirectory(GetErrorLogDirectory(web_apps_directory)))
    return Result::kError;

  base::FilePath log_file_name =
      GetErrorLogFileName(web_apps_directory, subsystem_name);

  JSONFileValueSerializer serializer(log_file_name);
  return serializer.Serialize(error_log) ? Result::kOk : Result::kError;
}

Result ClearErrorLogBlocking(scoped_refptr<FileUtilsWrapper> utils,
                             const base::FilePath& web_apps_directory,
                             std::string_view subsystem_name) {
  base::FilePath log_file_name =
      GetErrorLogFileName(web_apps_directory, subsystem_name);

  return utils->DeleteFile(log_file_name, /*recursive=*/false) ? Result::kOk
                                                               : Result::kError;
}

}  // namespace

void ReadErrorLog(const base::FilePath& web_apps_directory,
                  std::string_view subsystem_name,
                  ReadErrorLogCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, kTaskTraits,
      base::BindOnce(ReadErrorLogBlocking,
                     base::MakeRefCounted<FileUtilsWrapper>(),
                     web_apps_directory, subsystem_name),
      base::BindOnce(OnReadErrorLogBlocking, std::move(callback)));
}

void WriteErrorLog(const base::FilePath& web_apps_directory,
                   std::string_view subsystem_name,
                   base::Value error_log,
                   FileIoCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, kTaskTraits,
      base::BindOnce(WriteErrorLogBlocking,
                     base::MakeRefCounted<FileUtilsWrapper>(),
                     web_apps_directory, subsystem_name, std::move(error_log)),
      std::move(callback));
}

void ClearErrorLog(const base::FilePath& web_apps_directory,
                   std::string_view subsystem_name,
                   FileIoCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, kTaskTraits,
      base::BindOnce(ClearErrorLogBlocking,
                     base::MakeRefCounted<FileUtilsWrapper>(),
                     web_apps_directory, subsystem_name),
      std::move(callback));
}

}  // namespace web_app
