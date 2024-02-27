// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PRINTING_FAKE_PRINTER_CONFIG_CACHE_H_
#define CHROMEOS_PRINTING_FAKE_PRINTER_CONFIG_CACHE_H_

#include <string>
#include <string_view>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/time/clock.h"
#include "chromeos/printing/printer_config_cache.h"

namespace chromeos {

// A FakePrinterConfigCache provides canned responses like a real
// PrinterConfigCache would for testing purposes.
//
// This class doesn't meaningfully populate
// PrinterConfigCache::FetchResult::time_of_fetch.
class COMPONENT_EXPORT(CHROMEOS_PRINTING) FakePrinterConfigCache
    : public PrinterConfigCache {
 public:
  FakePrinterConfigCache();
  ~FakePrinterConfigCache() override;

  // Calls |cb| with a canned response for |key| previously provided by
  // SetFetchResponseForTesting().
  void Fetch(const std::string& key,
             base::TimeDelta unused_expiration,
             PrinterConfigCache::FetchCallback cb) override;

  // Causes subsequent Fetch() calls for |key| to fail (until a future
  // SetFetchResponseForTesting() provides a new canned response).
  void Drop(const std::string& key) override;

  // Sets internal state of |this| s.t. future Fetch() calls for
  // |key| get called back with |value|.
  // Subsequent calls to this method override the canned |value|.
  void SetFetchResponseForTesting(std::string_view key, std::string_view value);

  // Sets internal state of |this| s.t. future Fetch() calls for
  // |key| are consumed (i.e. delayed indefinitely and never called
  // back). The effects of this are undone by a subsequent call to
  // SetFetchResponseForTesting() or to Drop().
  //
  // This method is useful for simulating a slow server: one that
  // doesn't immediately respond to a Fetch() request (in fact, it
  // never responds at all, so use this carefully).
  void DiscardFetchRequestFor(std::string_view key);

 private:
  base::flat_map<std::string, std::string> contents_;
  base::flat_set<std::string> fetch_requests_to_ignore_;
};

}  // namespace chromeos

#endif  // CHROMEOS_PRINTING_FAKE_PRINTER_CONFIG_CACHE_H_
