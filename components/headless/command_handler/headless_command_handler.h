// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HEADLESS_COMMAND_HANDLER_HEADLESS_COMMAND_HANDLER_H_
#define COMPONENTS_HEADLESS_COMMAND_HANDLER_HEADLESS_COMMAND_HANDLER_H_

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "components/devtools/simple_devtools_protocol_client/simple_devtools_protocol_client.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

namespace headless {

class HeadlessCommandHandler : public content::WebContentsObserver {
 public:
  enum class Result {
    kSuccess,
    kPageLoadTimeout,
    kWriteFileError,
  };

  typedef base::OnceCallback<void(Result)> DoneCallback;

  HeadlessCommandHandler(const HeadlessCommandHandler&) = delete;
  HeadlessCommandHandler& operator=(const HeadlessCommandHandler&) = delete;

  static GURL GetHandlerUrl();

  static bool HasHeadlessCommandSwitches(const base::CommandLine& command_line);

  // The caller may override the TaskRunner used for file I/O by providing
  // a value for |io_task_runner|.
  static void ProcessCommands(
      content::WebContents* web_contents,
      GURL target_url,
      DoneCallback done_callback,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner = {});

  // Sets an additional callback that is fired when command is processed
  // for testing purposes.
  static void SetDoneCallbackForTesting(DoneCallback done_callback);

 private:
  using SimpleDevToolsProtocolClient =
      simple_devtools_protocol_client::SimpleDevToolsProtocolClient;

  HeadlessCommandHandler(
      content::WebContents* web_contents,
      GURL target_url,
      DoneCallback done_callback,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner);
  ~HeadlessCommandHandler() override;

  // content::WebContentsObserver implementation:
  void DocumentOnLoadCompletedInPrimaryMainFrame() override;
  void WebContentsDestroyed() override;

  void OnTargetCrashed(const base::Value::Dict&);

  void OnCommandsResult(base::Value::Dict result);

  void WriteFile(base::FilePath file_path, std::string base64_file_data);
  void OnWriteFileDone(bool success);

  void PostDone();
  void Done();

  SimpleDevToolsProtocolClient devtools_client_;
  SimpleDevToolsProtocolClient browser_devtools_client_;
  GURL target_url_;
  DoneCallback done_callback_;
  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;

  base::FilePath pdf_file_path_;
  base::FilePath screenshot_file_path_;

  int write_file_tasks_in_flight_ = 0;
  Result result_ = Result::kSuccess;
};

}  // namespace headless

#endif  // COMPONENTS_HEADLESS_COMMAND_HANDLER_HEADLESS_COMMAND_HANDLER_H_
