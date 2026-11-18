// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/importer/importer.h"

#include <utility>

#include "base/check.h"
#include "chrome/common/importer/importer_bridge.h"
#include "components/user_data_importer/mojom/bookmark_html_parser.mojom.h"

void Importer::Cancel() {
  cancelled_.store(true, std::memory_order_relaxed);
}

void Importer::SetBookmarkHtmlParser(
    mojo::PendingRemote<user_data_importer::mojom::BookmarkHtmlParser> parser) {
  bookmark_html_parser_remote_ = std::move(parser);
}

Importer::Importer() = default;

Importer::~Importer() = default;

std::unique_ptr<mojo::Remote<user_data_importer::mojom::BookmarkHtmlParser>>
Importer::BindBookmarkHtmlParser() {
  CHECK(bookmark_html_parser_remote_.is_valid());
  return std::make_unique<
      mojo::Remote<user_data_importer::mojom::BookmarkHtmlParser>>(
      std::move(bookmark_html_parser_remote_));
}
