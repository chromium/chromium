// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_PARSERS_JSON_PARSER_SANDBOXED_JSON_PARSER_H_
#define CHROME_CHROME_CLEANER_PARSERS_JSON_PARSER_SANDBOXED_JSON_PARSER_H_

#include "chrome/chrome_cleaner/ipc/mojo_task_runner.h"
#include "chrome/chrome_cleaner/mojom/parser_interface.mojom.h"
#include "chrome/chrome_cleaner/parsers/json_parser/json_parser_api.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chrome_cleaner {

// An implementation of JsonParserAPI to wrap a MojoTaskRunner and
// JsonParserPtr. Parses via |parser_| on the |mojo_task_runner_|.
// TODO(joenotcharles): Move this class to chrome_cleaner/parsers/broker.
class SandboxedJsonParser : public JsonParserAPI {
 public:
  SandboxedJsonParser(MojoTaskRunner* mojo_task_runner,
                      mojo::Remote<mojom::Parser>* parser);
  void Parse(const std::string& json, ParseDoneCallback callback) override;

 private:
  MojoTaskRunner* mojo_task_runner_;
  mojo::Remote<mojom::Parser>* parser_;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_PARSERS_JSON_PARSER_SANDBOXED_JSON_PARSER_H_
