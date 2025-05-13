// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_data_importer/common/importer_url_row.h"

namespace user_data_importer {

ImporterURLRow::ImporterURLRow()
    : visit_count(0), typed_count(0), hidden(false) {}

ImporterURLRow::ImporterURLRow(const GURL& url)
    : url(url), visit_count(0), typed_count(0), hidden(false) {}

ImporterURLRow::ImporterURLRow(const ImporterURLRow&) = default;

ImporterURLRow& ImporterURLRow::operator=(const ImporterURLRow&) = default;

}  // namespace user_data_importer
