// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HEADLESS_COMMAND_HANDLER_HEADLESS_COMMAND_SWITCHES_H_
#define COMPONENTS_HEADLESS_COMMAND_HANDLER_HEADLESS_COMMAND_SWITCHES_H_

namespace headless::switches {

extern const char kDefaultBackgroundColor[];
extern const char kDumpDom[];
extern const char kPrintToPDF[];
extern const char kNoPDFHeaderFooter[];
extern const char kDisablePDFTagging[];
extern const char kGeneratePDFDocumentOutline[];
extern const char kScreenshot[];
extern const char kTimeout[];
extern const char kVirtualTimeBudget[];

}  // namespace headless::switches

#endif  // COMPONENTS_HEADLESS_COMMAND_HANDLER_HEADLESS_COMMAND_SWITCHES_H_
