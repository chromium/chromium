// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The PrinterConfigCache class accepts requests to fetch things from
// the Chrome OS Printing serving root. It only stores things in memory.
//
// In practice, the present class fetches either PPDs or PPD metadata.

#ifndef CHROMEOS_PRINTING_PRINTER_CONFIG_CACHE_H_
#define CHROMEOS_PRINTING_PRINTER_CONFIG_CACHE_H_

#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/clock.h"
#include "base/time/time.h"

namespace network::mojom {
class URLLoaderFactory;
}

namespace chromeos {

// A PrinterConfigCache maps keys to values. By convention, keys are
// relative paths to files in the Chrome OS Printing serving root
// (hardcoded into this class). In practice, that means keys will either
// *  start with "metadata_v3/" or
// *  start with "ppds_for_metadata_v3/."
//
// This class must always be constructed on, used on, and destroyed from
// a sequenced context.
class COMPONENT_EXPORT(CHROMEOS_PRINTING) PrinterConfigCache {
 public:
  // |loader_factory_dispenser| is a functor that can create fresh
  // URLLoaderFactory instances. We use this indirection to avoid
  // caching raw pointers to URLLoaderFactory instances, which are
  // invalidated by network service restarts.
  //
  // Caller must guarantee that |loader_factory_dispenser| is always
  // safe to Run() for the lifetime of |this|.
  //
  // Setting `use_localhost_as_root` to true sets the Chrome OS Printing
  // serving root to localhost. It allows to run integration tests without
  // connecting to actual Chrome OS Printing serving root.
  static std::unique_ptr<PrinterConfigCache> Create(
      const base::Clock* clock,
      base::RepeatingCallback<network::mojom::URLLoaderFactory*()>
          loader_factory_dispenser,
      bool use_localhost_as_root);
  virtual ~PrinterConfigCache() = default;

  // Result of calling Fetch(). The |key| identifies how Fetch() was
  // originally invoked. The |contents| and |time_of_fetch| are well-
  // defined iff |succeeded| is true.
  struct FetchResult {
    static FetchResult Failure(const std::string& key);
    static FetchResult Success(const std::string& key,
                               const std::string& contents,
                               base::Time time_of_fetch);
    bool succeeded;
    std::string key;
    std::string contents;
    base::Time time_of_fetch;
  };

  // Caller is responsible for providing sequencing of this type.
  using FetchCallback = base::OnceCallback<void(const FetchResult&)>;

  // Queries the Chrome OS Printing serving root for |key|. Calls |cb|
  // with the contents. If an entry newer than |expiration| is resident,
  // calls |cb| immediately with those contents. Caller should not pass
  // keys with leading slashes.
  //
  // Using TimeDelta implies the caller is asking for "some entry not
  // older than |expiration|," e.g. "metadata_v3/index-00.json that
  // was fetched within the last 30 minutes."
  //
  // Naturally,
  // *  passing the Max() TimeDelta means "perform this Fetch() with no
  //    limit on staleness" and
  // *  passing a zero TimeDelta should practically force a networked
  //    fetch (less esoteric timing quirks etc.).
  virtual void Fetch(const std::string& key,
                     base::TimeDelta expiration,
                     FetchCallback cb) = 0;

  // Drops Entry corresponding to |key|.
  virtual void Drop(const std::string& key) = 0;
};

}  // namespace chromeos

#endif  // CHROMEOS_PRINTING_PRINTER_CONFIG_CACHE_H_
