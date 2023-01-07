// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_MEDIA_ROUTER_PRESENTATION_REQUEST_SOURCE_OBSERVER_H_
#define CHROME_BROWSER_UI_MEDIA_ROUTER_PRESENTATION_REQUEST_SOURCE_OBSERVER_H_

namespace media_router {

// This interface is used for observing updates from |MediaRouteStarter| when
// the presentation source for casting changes.
class PresentationRequestSourceObserver {
 public:
  virtual ~PresentationRequestSourceObserver() = default;

  virtual void OnSourceUpdated(std::u16string& source_name) = 0;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_MEDIA_ROUTER_PRESENTATION_REQUEST_SOURCE_OBSERVER_H_
