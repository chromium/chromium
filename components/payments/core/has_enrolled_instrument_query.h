// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_HAS_ENROLLED_INSTRUMENT_QUERY_H_
#define COMPONENTS_PAYMENTS_CORE_HAS_ENROLLED_INSTRUMENT_QUERY_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "components/keyed_service/core/keyed_service.h"

class GURL;

namespace payments {

// Keeps track of hasEnrolledInstrument() queries per browser context.
class HasEnrolledInstrumentQuery : public KeyedService {
 public:
  HasEnrolledInstrumentQuery();

  HasEnrolledInstrumentQuery(const HasEnrolledInstrumentQuery&) = delete;
  HasEnrolledInstrumentQuery& operator=(
      const HasEnrolledInstrumentQuery&) = delete;

  ~HasEnrolledInstrumentQuery() override;

  // Returns whether |top_level_origin| and |frame_origin| can call
  // hasEnrolledInstrument() with |query|, which is a mapping of payment method
  // names to the corresponding JSON-stringified payment method data. Remembers
  // the origins-to-query mapping for 30 minutes to enforce the quota.
  //
  // GURL type is used instead of url::Origin to represent origins, because they
  // need to be serialized into map keys and url::Origin serializations must not
  // be relied upon for security checks, according to url/origin.h.
  //
  // The best method to retrieve the origin of GURL for serialization is
  // url_formatter::FormatUrlForSecurityDisplay() found in
  // components/url_formatter/elide_url.h, because it preserves the path part of
  // a file:// scheme GURL, in contrast to to GURL::GetOrigin(), which strips
  // the path part of a file:// scheme GURL. There's no difference between these
  // two methods for localhost and https:// schemes, where PaymentRequest is
  // also allowed.
  bool CanQuery(const GURL& top_level_origin,
                const GURL& frame_origin,
                const std::map<std::string, std::set<std::string>>& query);

  // KeyedService implementation.
  void Shutdown() override;

 private:
  void ExpireQuotaForFrameOrigin(const std::string& id);

  // A mapping of an identififer to the timer that, when fired, allows the frame
  // to invoke hasEnrolledInstrument() with the same identifier again.
  std::map<std::string, std::unique_ptr<base::OneShotTimer>> timers_;

  // A mapping of frame origin and top level origin to its last query. Each
  // query is a mapping of payment method names to the corresponding
  // JSON-stringified payment method data.
  std::map<std::string, std::map<std::string, std::set<std::string>>> queries_;

  base::WeakPtrFactory<HasEnrolledInstrumentQuery> weak_ptr_factory_{this};
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_HAS_ENROLLED_INSTRUMENT_QUERY_H_
