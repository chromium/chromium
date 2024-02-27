// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/printing/fake_printer_config_cache.h"

#include <string>
#include <string_view>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chromeos/printing/printer_config_cache.h"

namespace chromeos {

FakePrinterConfigCache::FakePrinterConfigCache() = default;
FakePrinterConfigCache::~FakePrinterConfigCache() = default;

void FakePrinterConfigCache::SetFetchResponseForTesting(
    std::string_view key,
    std::string_view value) {
  contents_.insert_or_assign(std::string(key), std::string(value));

  // If Fetch(|key|) was previously being consumed by prior call to
  // DiscardFetchRequestFor(), we unblock it now.
  fetch_requests_to_ignore_.erase(key);
}

void FakePrinterConfigCache::DiscardFetchRequestFor(std::string_view key) {
  fetch_requests_to_ignore_.insert(std::string(key));
  contents_.erase(key);
}

void FakePrinterConfigCache::Fetch(const std::string& key,
                                   base::TimeDelta unused_expiration,
                                   PrinterConfigCache::FetchCallback cb) {
  if (contents_.contains(key)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(cb),
                                  PrinterConfigCache::FetchResult::Success(
                                      key, contents_.at(key), base::Time())));
    return;
  } else if (fetch_requests_to_ignore_.contains(key)) {
    // Caller has directed us, by way of DiscardFetchRequestFor(), to
    // _not_ respond to this Fetch().
    return;
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(cb),
                                PrinterConfigCache::FetchResult::Failure(key)));
}

void FakePrinterConfigCache::Drop(const std::string& key) {
  contents_.erase(key);

  // If Fetch(|key|) was previously being consumed by prior call to
  // DiscardFetchRequestFor(), we unblock it now.
  fetch_requests_to_ignore_.erase(key);
}

}  // namespace chromeos
