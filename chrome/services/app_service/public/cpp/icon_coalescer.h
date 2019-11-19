// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_APP_SERVICE_PUBLIC_CPP_ICON_COALESCER_H_
#define CHROME_SERVICES_APP_SERVICE_PUBLIC_CPP_ICON_COALESCER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/services/app_service/public/cpp/icon_loader.h"

namespace apps {

// An IconLoader that coalesces the apps::mojom::IconCompression::kUncompressed
// results of another (wrapped) IconLoader.
//
// This is similar to, but different from, an IconCache. Both types are related
// to the LoadIconFromIconKey Mojo call (the request and response), both reduce
// the number of requests made, and both re-use the response for requests with
// the same IconLoader::Key.
//
// An IconCache (another class) applies when the second request is sent *after*
// the first response is received. An IconCoalescer (this class) applies when
// the second request is sent *before* the first response is received (but
// after the first request is sent, obviously).
//
// Caching means that the second (and subsequent) requests can be satisfied
// immediately, sharing the previous response. Coalescing means that the second
// (and subsequent) requests are paused, and when the first request's response
// is finally received, those other requests are un-paused and share the same
// response.
//
// When there are no in-flight requests, a (memory-backed) cache can still have
// a significant memory cost, depending on how aggressive its cache eviction
// policy is, but a (memory-backed) coalescer will have a trivial memory cost.
// Much of its internal state (e.g. maps and multimaps) will be empty.
class IconCoalescer : public IconLoader {
 public:
  explicit IconCoalescer(IconLoader* wrapped_loader);
  ~IconCoalescer() override;

  // IconLoader overrides.
  apps::mojom::IconKeyPtr GetIconKey(const std::string& app_id) override;
  std::unique_ptr<IconLoader::Releaser> LoadIconFromIconKey(
      apps::mojom::AppType app_type,
      const std::string& app_id,
      apps::mojom::IconKeyPtr icon_key,
      apps::mojom::IconCompression icon_compression,
      int32_t size_hint_in_dip,
      bool allow_placeholder_icon,
      apps::mojom::Publisher::LoadIconCallback callback) override;

 private:
  class RefCountedReleaser;

  using CallbackAndReleaser =
      std::pair<apps::mojom::Publisher::LoadIconCallback,
                scoped_refptr<RefCountedReleaser>>;

  void OnLoadIcon(IconLoader::Key,
                  uint64_t sequence_number,
                  apps::mojom::IconValuePtr);

  IconLoader* wrapped_loader_;

  // Every incoming LoadIconFromIconKey call gets its own sequence number.
  uint64_t next_sequence_number_;

  // Sequence numbers for outstanding requests to to the wrapped_loader_'s
  // LoadIconFromIconKey. When the wrapped_loader_ returns, there will either
  // be a matching entry in immediate_responses_ or an entry will be made in
  // non_immediate_requests_, depending on whether LoadIconFromIconKey resolved
  // (ran its callback) synchronously (immediately) or asynchronously
  // (non-immediately).
  std::set<uint64_t> possibly_immediate_requests_;

  // Map from sequence number to the IconValue to give to the LoadIconCallback.
  std::map<uint64_t, apps::mojom::IconValuePtr> immediate_responses_;

  // Multimap of pending LoadIconFromIconKey calls: those calls that were not
  // resolved immediately.
  std::multimap<IconLoader::Key, CallbackAndReleaser> non_immediate_requests_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<IconCoalescer> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(IconCoalescer);
};

}  // namespace apps

#endif  // CHROME_SERVICES_APP_SERVICE_PUBLIC_CPP_ICON_COALESCER_H_
