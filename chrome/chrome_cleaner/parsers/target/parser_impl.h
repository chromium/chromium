// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_PARSERS_TARGET_PARSER_IMPL_H_
#define CHROME_CHROME_CLEANER_PARSERS_TARGET_PARSER_IMPL_H_

#include "chrome/chrome_cleaner/mojom/parser_interface.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chrome_cleaner {

class ParserImpl : public mojom::Parser {
 public:
  explicit ParserImpl(mojo::PendingReceiver<mojom::Parser> receiver,
                      base::OnceClosure connection_error_handler);
  ~ParserImpl() override;

  // mojom::Parser
  void ParseJson(const std::string& json,
                 ParserImpl::ParseJsonCallback callback) override;

  // mojom:Parser
  void ParseShortcut(mojo::ScopedHandle lnk_file_handle,
                     ParserImpl::ParseShortcutCallback callback) override;

 private:
  mojo::Receiver<mojom::Parser> receiver_;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_PARSERS_TARGET_PARSER_IMPL_H_
