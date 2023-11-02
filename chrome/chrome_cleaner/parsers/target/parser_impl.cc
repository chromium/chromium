// Copyright 2018 The Chromium Authors
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
  auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(
      json, base::JSON_PARSE_CHROMIUM_EXTENSIONS |
                base::JSON_ALLOW_TRAILING_COMMAS |
                base::JSON_REPLACE_INVALID_CHARACTERS);
  if (parsed_json.has_value()) {
    std::move(callback).Run(std::move(*parsed_json), absl::nullopt);
  } else {
    std::move(callback).Run(
        absl::nullopt,
        absl::make_optional(std::move(parsed_json.error().message)));
  }
}

void ParserImpl::ParseShortcut(mojo::PlatformHandle lnk_file_handle,
                               ParserImpl::ParseShortcutCallback callback) {
  base::win::ScopedHandle shortcut_handle = lnk_file_handle.TakeHandle();
  if (!shortcut_handle.IsValid()) {
    LOG(ERROR) << "Unable to get raw file HANDLE from mojo.";
    std::move(callback).Run(mojom::LnkParsingResult::INVALID_HANDLE,
                            absl::make_optional<std::wstring>(),
                            absl::make_optional<std::wstring>(),
                            absl::make_optional<std::wstring>(),
                            absl::make_optional<std::wstring>(),
                            /*icon_index=*/-1);
    return;
  }

  ParsedLnkFile parsed_shortcut;
  mojom::LnkParsingResult result =
      ParseLnk(std::move(shortcut_handle), &parsed_shortcut);

  if (result != mojom::LnkParsingResult::SUCCESS) {
    LOG(ERROR) << "Error parsing the shortcut";
    std::move(callback).Run(result, absl::make_optional<std::wstring>(),
                            absl::make_optional<std::wstring>(),
                            absl::make_optional<std::wstring>(),
                            absl::make_optional<std::wstring>(),
                            /*icon_index=*/-1);
    return;
  }
  std::move(callback).Run(
      result, absl::make_optional<std::wstring>(parsed_shortcut.target_path),
      absl::make_optional<std::wstring>(parsed_shortcut.working_dir),
      absl::make_optional<std::wstring>(parsed_shortcut.command_line_arguments),
      absl::make_optional<std::wstring>(parsed_shortcut.icon_location),
      parsed_shortcut.icon_index);
}

}  // namespace chrome_cleaner
