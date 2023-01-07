// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_SEARCH_PROVIDER_OBSERVER_H_
#define COMPONENTS_SEARCH_SEARCH_PROVIDER_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_observer.h"

// Keeps track of any changes in search engine provider and call
// the provided callback if a third-party search provider (i.e. a third-party
// NTP) is being used.
class SearchProviderObserver : public TemplateURLServiceObserver {
 public:
  explicit SearchProviderObserver(TemplateURLService* service,
                                  base::RepeatingClosure callback);
  ~SearchProviderObserver() override;

  virtual bool is_google();

 private:
  // TemplateURLServiceObserver:
  void OnTemplateURLServiceChanged() override;
  void OnTemplateURLServiceShuttingDown() override;

  base::ScopedObservation<TemplateURLService, TemplateURLServiceObserver>
      service_observation_{this};
  // May be nullptr in tests.
  raw_ptr<TemplateURLService> service_;
  bool is_google_;
  base::RepeatingClosure callback_;
};

#endif  // COMPONENTS_SEARCH_SEARCH_PROVIDER_OBSERVER_H_
