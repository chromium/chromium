// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/ipc_interfaces_dumper.h"

#include <set>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/environment.h"
#include "base/files/file.h"
#include "base/json/json_writer.h"
#include "base/strings/escape.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"

// Format for the outputted JSON:
// {
//   # Those are the context (frame, document...) bound interfaces.
//   "context_interfaces": [
//     {
//       "qualified_name": "blah",
//       "type": "{Associated,}Remote"
//     },
//     ...
//   ],
//   # Those are the process bound interfaces.
//   "process_interfaces": [
//     {
//       "qualified_name": "blah",
//       "type": "{Associated,}Remote"
//     },
//     ...
//   ]
// }

namespace {

void RegisterInterfaces(const std::vector<std::string>& interfaces,
                        const std::string& type,
                        base::Value::List* list) {
  // Remove duplicates.
  std::set<std::string> unique_interfaces(interfaces.begin(), interfaces.end());

  for (auto& name : unique_interfaces) {
    base::Value::Dict entry;
    entry.Set("qualified_name", name);
    entry.Set("type", type);
    list->Append(std::move(entry));
  }
}

}  // namespace

class IPCInterfacesDumper : public InProcessBrowserTest {
 public:
  IPCInterfacesDumper() = default;
};

IN_PROC_BROWSER_TEST_F(IPCInterfacesDumper, DumperTest) {
  auto env = base::Environment::Create();
  if (!env->HasVar("IPC_DUMP_PATH")) {
    LOG(ERROR) << "IPC_DUMP_PATH not set. Nothing will be done.";
    return;
  }

  content::RenderFrameHost* rfh = browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetPrimaryMainFrame();

  std::vector<std::string> rfh_interfaces;
  std::vector<std::string> rfh_interfaces_associated;
  std::vector<std::string> process_interfaces;

  content::GetBoundInterfacesForTesting(rfh, rfh_interfaces);
  content::GetBoundAssociatedInterfacesForTesting(rfh,
                                                  rfh_interfaces_associated);
  content::GetBoundInterfacesForTesting(rfh->GetProcess(), process_interfaces);

  base::Value::List context_interfaces;
  base::Value::List process_interface;
  RegisterInterfaces(rfh_interfaces, "Remote", &context_interfaces);
  RegisterInterfaces(rfh_interfaces_associated, "AssociatedRemote",
                     &context_interfaces);
  RegisterInterfaces(process_interfaces, "Remote", &process_interface);

  base::Value::Dict json;
  json.Set("context_interfaces", std::move(context_interfaces));
  json.Set("process_interfaces", std::move(process_interface));

  // Write the JSON to a file in the IPC_DUMP_PATH directory.
  std::string file_path;
  env->GetVar("IPC_DUMP_PATH", &file_path);

  base::ScopedAllowBlockingForTesting allow_blocking;
#if BUILDFLAG(IS_WIN)
  base::FilePath filepath = base::FilePath(base::UTF8ToWide(file_path));
#else
  base::FilePath filepath = base::FilePath(file_path);
#endif
  base::File file(std::move(filepath),
                  base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  std::optional<std::string> json_string = base::WriteJson(json);
  CHECK(json_string.has_value());
  file.WriteAtCurrentPos(base::as_byte_span(json_string.value()));
}
