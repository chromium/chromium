// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_EXTENSIONS_API_HISTORY_HISTORY_API_H_
#define CHROMECAST_BROWSER_EXTENSIONS_API_HISTORY_HISTORY_API_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chromecast/common/extensions_api/history.h"
#include "extensions/browser/extension_function.h"

namespace extensions {
namespace cast {
namespace api {

class HistoryGetVisitsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("history.getVisits", HISTORY_GETVISITS)

 protected:
  ~HistoryGetVisitsFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

class HistorySearchFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("history.search", HISTORY_SEARCH)

 protected:
  ~HistorySearchFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

class HistoryAddUrlFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("history.addUrl", HISTORY_ADDURL)

 protected:
  ~HistoryAddUrlFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

class HistoryDeleteAllFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("history.deleteAll", HISTORY_DELETEALL)

 protected:
  ~HistoryDeleteAllFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

class HistoryDeleteUrlFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("history.deleteUrl", HISTORY_DELETEURL)

 protected:
  ~HistoryDeleteUrlFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

class HistoryDeleteRangeFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("history.deleteRange", HISTORY_DELETERANGE)

 protected:
  ~HistoryDeleteRangeFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

}  // namespace api
}  // namespace cast
}  // namespace extensions

#endif  // CHROMECAST_BROWSER_EXTENSIONS_API_HISTORY_HISTORY_API_H_
