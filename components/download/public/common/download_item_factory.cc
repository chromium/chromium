// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The DownloadItemFactory is used to produce different DownloadItems.
// It is separate from the DownloadManager to allow download manager
// unit tests to control the items produced.

#include "components/download/public/common/download_item_factory.h"

namespace download {

DownloadItemFactory::~DownloadItemFactory() = default;

}  // namespace download
