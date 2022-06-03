// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICE_CLOUD_PRINT_CDD_CONVERSION_WIN_H_
#define CHROME_SERVICE_CLOUD_PRINT_CDD_CONVERSION_WIN_H_

#include <windows.h>

#include <memory>
#include <string>

#include "base/memory/free_deleter.h"

namespace cloud_print {

bool IsValidCjt(const std::string& print_ticket);

std::unique_ptr<DEVMODE, base::FreeDeleter> CjtToDevMode(
    const std::wstring& printer_name,
    const std::string& print_ticket);

}  // namespace cloud_print

#endif  // CHROME_SERVICE_CLOUD_PRINT_CDD_CONVERSION_WIN_H_
