// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/parsers/json_parser/sandboxed_json_parser.h"

#include <utility>

#include "base/bind.h"

namespace chrome_cleaner {

SandboxedJsonParser::SandboxedJsonParser(MojoTaskRunner* mojo_task_runner,
                                         mojo::Remote<mojom::Parser>* parser)
    : mojo_task_runner_(mojo_task_runner), parser_(parser) {}

void SandboxedJsonParser::Parse(const std::string& json,
                                ParseDoneCallback callback) {
  mojo_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(
                     [](mojo::Remote<mojom::Parser>* parser,
                        const std::string& json, ParseDoneCallback callback) {
                       (*parser)->ParseJson(json, std::move(callback));
                     },
                     parser_, json, std::move(callback)));
}

}  // namespace chrome_cleaner
