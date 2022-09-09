// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/cups_proxy/cups_proxy_service_delegate.h"

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"

namespace cups_proxy {

CupsProxyServiceDelegate::CupsProxyServiceDelegate() {}
CupsProxyServiceDelegate::~CupsProxyServiceDelegate() = default;

base::WeakPtr<CupsProxyServiceDelegate> CupsProxyServiceDelegate::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace cups_proxy
