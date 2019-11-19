// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_SERVICE_OBSERVER_H_
#define COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_SERVICE_OBSERVER_H_

#include "base/observer_list_types.h"

// TemplateURLServiceObserver is notified whenever the set of TemplateURLs
// are modified.
class TemplateURLServiceObserver : public base::CheckedObserver {
 public:
  // Notification that the template url model has changed in some way.
  virtual void OnTemplateURLServiceChanged() = 0;

  // Notification that the template url service is shutting down. Observers that
  // might outlive the service can use this to clear out any raw pointers to the
  // service.
  virtual void OnTemplateURLServiceShuttingDown() {}

 protected:
  ~TemplateURLServiceObserver() override {}
};

#endif  // COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_SERVICE_OBSERVER_H_
