// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/cloud_printer_handler.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/values.h"

namespace printing {

CloudPrinterHandler::CloudPrinterHandler() {}

CloudPrinterHandler::~CloudPrinterHandler() {}

void CloudPrinterHandler::Reset() {}

void CloudPrinterHandler::StartGetPrinters(
    AddedPrintersCallback added_printers_callback,
    GetPrintersDoneCallback done_callback) {
  // TODO(https://crbug.com/829414): Actually retrieve the printers
  std::move(done_callback).Run();
}

void CloudPrinterHandler::StartGetCapability(const std::string& destination_id,
                                             GetCapabilityCallback callback) {
  // TODO(https://crbug.com/829414): Get capabilities.
  std::move(callback).Run(base::Value());
}

void CloudPrinterHandler::StartPrint(
    const base::string16& job_title,
    base::Value settings,
    scoped_refptr<base::RefCountedMemory> print_data,
    PrintCallback callback) {
  // TODO(https://crbug.com/829414): Print to cloud print
  NOTIMPLEMENTED();
}

}  // namespace printing
