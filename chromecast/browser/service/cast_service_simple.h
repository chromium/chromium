// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_SERVICE_CAST_SERVICE_SIMPLE_H_
#define CHROMECAST_BROWSER_SERVICE_CAST_SERVICE_SIMPLE_H_

#include "base/memory/weak_ptr.h"
#include "chromecast/browser/cast_content_window.h"
#include "chromecast/browser/cast_web_view.h"
#include "chromecast/service/cast_service.h"
#include "url/gurl.h"

namespace chromecast {

class CastWebService;

namespace shell {

class CastServiceSimple : public CastService {
 public:
  explicit CastServiceSimple(CastWebService* web_service);

  CastServiceSimple(const CastServiceSimple&) = delete;
  CastServiceSimple& operator=(const CastServiceSimple&) = delete;

  ~CastServiceSimple() override;

 protected:
  // CastService implementation:
  void InitializeInternal() override;
  void FinalizeInternal() override;
  void StartInternal() override;
  void StopInternal() override;

 private:
  CastWebService* const web_service_;
  CastWebView::Scoped cast_web_view_;
  GURL startup_url_;

  base::WeakPtrFactory<CastServiceSimple> weak_factory_{this};
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_SERVICE_CAST_SERVICE_SIMPLE_H_
