// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_HATS_TRUST_SAFETY_SENTIMENT_SERVICE_H_
#define CHROME_BROWSER_UI_HATS_TRUST_SAFETY_SENTIMENT_SERVICE_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

// Service which receives events from Trust & Safety features and determines
// whether or not to launch a HaTS survey on the NTP for the user.
class TrustSafetySentimentService : public KeyedService {
 public:
  explicit TrustSafetySentimentService(Profile* profile);
  ~TrustSafetySentimentService() override;
};

#endif  // CHROME_BROWSER_UI_HATS_TRUST_SAFETY_SENTIMENT_SERVICE_H_
