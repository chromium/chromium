// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/importer/importer.h"

#include "chrome/common/importer/importer_bridge.h"
#include "components/user_data_importer/mojom/bookmark_html_parser.mojom.h"

void Importer::Cancel() {
  cancelled_ = true;
}

void Importer::SetBookmarkHtmlParser(
    mojo::PendingRemote<user_data_importer::mojom::BookmarkHtmlParser> parser) {
}

Importer::Importer() : cancelled_(false) {}

Importer::~Importer() = default;
