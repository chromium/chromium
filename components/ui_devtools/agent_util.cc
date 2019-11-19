// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/agent_util.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"

namespace ui_devtools {

namespace {

void OnSourceFile(base::OnceClosure quit_closure,
                  bool* return_value,
                  bool read_file_result) {
  *return_value = read_file_result;
  std::move(quit_closure).Run();
}

}  // namespace

const char kChromiumCodeSearchURL[] = "https://cs.chromium.org/";
const char kChromiumCodeSearchSrcURL[] =
    "https://cs.chromium.org/chromium/src/";

bool GetSourceCode(std::string path, std::string* source_code) {
  base::FilePath src_dir;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &src_dir);
  src_dir = src_dir.AppendASCII(path);

  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);

  bool return_value;
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&base::ReadFileToString, src_dir, source_code),
      base::BindOnce(&OnSourceFile, run_loop.QuitClosure(), &return_value));

  run_loop.Run();

  if (!return_value)
    DLOG(ERROR) << "Could not get source file of " << src_dir.value() << ".";

  return return_value;
}

}  // namespace ui_devtools
