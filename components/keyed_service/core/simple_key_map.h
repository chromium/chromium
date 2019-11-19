// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_KEYED_SERVICE_CORE_SIMPLE_KEY_MAP_H_
#define COMPONENTS_KEYED_SERVICE_CORE_SIMPLE_KEY_MAP_H_

#include <map>

#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/keyed_service/core/keyed_service_export.h"

namespace content {
class BrowserContext;
}  // namespace content

class SimpleFactoryKey;

// Stores a mapping from BrowserContexts to SimpleFactoryKeys.
//
// Use this class to get the SimpleFactoryKey that is associated with a given
// BrowserContext, when the BrowserContext is available and a
// SimpleKeyedServiceFactory for that BrowserContext is needed. For example,
// inside BuildServiceInstanceFor() in a BrowserContextKeyedServiceFactory that
// depends on a SimpleKeyedServiceFactory.
//
// This mapping is not stored as a member in BrowserContext because
// SimpleFactoryKeys are not a content layer concept, but a components level
// concept.
class KEYED_SERVICE_EXPORT SimpleKeyMap {
 public:
  static SimpleKeyMap* GetInstance();

  // When |browser_context| creates or takes ownership of a SimpleFactoryKey
  // |key|, it should register this association in this map.
  void Associate(content::BrowserContext* browser_context,
                 SimpleFactoryKey* key);

  // When |browser_context| is destroyed or loses ownership of a
  // SimpleFactoryKey, it should erase its association from this map.
  void Dissociate(content::BrowserContext* browser_context);

  // Gets the SimpleFactoryKey associated with |browser_context|.
  SimpleFactoryKey* GetForBrowserContext(
      content::BrowserContext* browser_context);

 private:
  friend class base::NoDestructor<SimpleKeyMap>;

  SimpleKeyMap();
  ~SimpleKeyMap();

  std::map<content::BrowserContext*, SimpleFactoryKey*> mapping_;

  DISALLOW_COPY_AND_ASSIGN(SimpleKeyMap);
};

#endif  // COMPONENTS_KEYED_SERVICE_CORE_SIMPLE_KEY_MAP_H_
