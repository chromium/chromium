// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_SERVICES_H_
#define CHROME_UTILITY_SERVICES_H_

namespace mojo {
class ServiceFactory;
}

// Helpers to run out-of-process services in a dedicated utility process. All
// out-of-process services will need to have their implementation hooked up in
// one of these helpers.
void RegisterElevatedMainThreadServices(mojo::ServiceFactory& services);
void RegisterMainThreadServices(mojo::ServiceFactory& services);
void RegisterIOThreadServices(mojo::ServiceFactory& services);

#endif  // CHROME_UTILITY_SERVICES_H_
