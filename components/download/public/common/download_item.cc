// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/download_item.h"

#include "services/network/public/cpp/shared_url_loader_factory.h"

#include "base/check.h"
#include "base/notreached.h"

namespace download {

DownloadItem::Observer::~Observer() {
  CHECK(!IsInObserverList());
}

void DownloadItem::SetStateForTesting(DownloadState state) {
  NOTREACHED();
}

void DownloadItem::SetDownloadUrlForTesting(const GURL& url) {
  NOTREACHED();
}

void DownloadItem::SetURLLoaderFactory(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {}


}  // namespace download
