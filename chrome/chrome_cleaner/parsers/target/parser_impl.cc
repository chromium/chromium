// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/parsers/target/parser_impl.h"

#include <utility>

#include "base/json/json_reader.h"
#include "base/values.h"
#include "base/win/scoped_handle.h"
#include "chrome/chrome_cleaner/parsers/shortcut_parser/target/lnk_parser.h"

namespace chrome_cleaner {

ParserImpl::ParserImpl(mojo::PendingReceiver<mojom::Parser> receiver,
                       base::OnceClosure connection_error_handler)
    : receiver_(this, std::move(receiver)) {
  receiver_.set_disconnect_handler(std::move(connection_error_handler));
}

ParserImpl::~ParserImpl() = default;

void ParserImpl::ParseJson(const std::string& json,
                           ParseJsonCallback callback) {
  base::JSONReader::ValueWithError parsed_json =
      base::JSONReader::ReadAndReturnValueWithError(
          json, base::JSON_ALLOW_TRAILING_COMMAS |
                    base::JSON_REPLACE_INVALID_CHARACTERS);
  if (parsed_json.value) {
    std::move(callback).Run(std::move(parsed_json.value), base::nullopt);
  } else {
    std::move(callback).Run(
        base::nullopt,
        base::make_optional(std::move(parsed_json.error_message)));
  }
}

void ParserImpl::ParseShortcut(mojo::PlatformHandle lnk_file_handle,
                               ParserImpl::ParseShortcutCallback callback) {
  base::win::ScopedHandle shortcut_handle = lnk_file_handle.TakeHandle();
  if (!shortcut_handle.IsValid()) {
    LOG(ERROR) << "Unable to get raw file HANDLE from mojo.";
    std::move(callback).Run(mojom::LnkParsingResult::INVALID_HANDLE,
                            base::make_optional<std::wstring>(),
                            base::make_optional<std::wstring>(),
                            base::make_optional<std::wstring>(),
                            base::make_optional<std::wstring>(),
                            /*icon_index=*/-1);
    return;
  }

  ParsedLnkFile parsed_shortcut;
  mojom::LnkParsingResult result =
      ParseLnk(std::move(shortcut_handle), &parsed_shortcut);

  if (result != mojom::LnkParsingResult::SUCCESS) {
    LOG(ERROR) << "Error parsing the shortcut";
    std::move(callback).Run(result, base::make_optional<std::wstring>(),
                            base::make_optional<std::wstring>(),
                            base::make_optional<std::wstring>(),
                            base::make_optional<std::wstring>(),
                            /*icon_index=*/-1);
    return;
  }
  std::move(callback).Run(
      result, base::make_optional<std::wstring>(parsed_shortcut.target_path),
      base::make_optional<std::wstring>(parsed_shortcut.working_dir),
      base::make_optional<std::wstring>(parsed_shortcut.command_line_arguments),
      base::make_optional<std::wstring>(parsed_shortcut.icon_location),
      parsed_shortcut.icon_index);
}

}  // namespace chrome_cleaner
