// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/parsers/target/parser_impl.h"

#include <utility>

#include "base/json/json_reader.h"
#include "base/values.h"
#include "base/win/scoped_handle.h"
#include "chrome/chrome_cleaner/parsers/shortcut_parser/target/lnk_parser.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace chrome_cleaner {

ParserImpl::ParserImpl(mojo::PendingReceiver<mojom::Parser> receiver,
                       base::OnceClosure connection_error_handler)
    : receiver_(this, std::move(receiver)) {
  receiver_.set_disconnect_handler(std::move(connection_error_handler));
}

ParserImpl::~ParserImpl() = default;

void ParserImpl::ParseJson(const std::string& json,
                           ParseJsonCallback callback) {
  int error_code;
  std::string error;
  std::unique_ptr<base::Value> value =
      base::JSONReader::ReadAndReturnErrorDeprecated(
          json,
          base::JSON_ALLOW_TRAILING_COMMAS |
              base::JSON_REPLACE_INVALID_CHARACTERS,
          &error_code, &error);
  if (value) {
    std::move(callback).Run(base::make_optional(std::move(*value)),
                            base::nullopt);
  } else {
    std::move(callback).Run(base::nullopt,
                            base::make_optional(std::move(error)));
  }
}

void ParserImpl::ParseShortcut(mojo::ScopedHandle lnk_file_handle,
                               ParserImpl::ParseShortcutCallback callback) {
  HANDLE raw_shortcut_handle;
  if (mojo::UnwrapPlatformFile(std::move(lnk_file_handle),
                               &raw_shortcut_handle) != MOJO_RESULT_OK) {
    LOG(ERROR) << "Unable to get raw file HANDLE from mojo.";
    std::move(callback).Run(mojom::LnkParsingResult::INVALID_HANDLE,
                            base::make_optional<base::string16>(),
                            base::make_optional<base::string16>(),
                            base::make_optional<base::string16>());
    return;
  }

  base::win::ScopedHandle shortcut_handle(raw_shortcut_handle);

  ParsedLnkFile parsed_shortcut;
  mojom::LnkParsingResult result =
      ParseLnk(std::move(shortcut_handle), &parsed_shortcut);

  if (result != mojom::LnkParsingResult::SUCCESS) {
    LOG(ERROR) << "Error parsing the shortcut";
    std::move(callback).Run(result, base::make_optional<base::string16>(),
                            base::make_optional<base::string16>(),
                            base::make_optional<base::string16>());
    return;
  }

  std::move(callback).Run(
      result, base::make_optional<base::string16>(parsed_shortcut.target_path),
      base::make_optional<base::string16>(
          parsed_shortcut.command_line_arguments),
      base::make_optional<base::string16>(parsed_shortcut.icon_location));
}

}  // namespace chrome_cleaner
