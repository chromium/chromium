// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/import/import_results.h"

namespace password_manager {

ImportEntry::ImportEntry() = default;
ImportEntry::ImportEntry(const ImportEntry& other) = default;
ImportEntry::ImportEntry(ImportEntry&& other) = default;
ImportEntry::~ImportEntry() = default;
ImportEntry& ImportEntry::operator=(const ImportEntry& entry) = default;
ImportEntry& ImportEntry::operator=(ImportEntry&& entry) = default;

ImportResults::ImportResults() = default;
ImportResults::ImportResults(const ImportResults& other) = default;
ImportResults::ImportResults(ImportResults&& other) = default;
ImportResults::~ImportResults() = default;
ImportResults& ImportResults::operator=(const ImportResults& entry) = default;
ImportResults& ImportResults::operator=(ImportResults&& entry) = default;

}  // namespace password_manager
