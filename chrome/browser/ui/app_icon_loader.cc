// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_icon_loader.h"

AppIconLoader::AppIconLoader() {}

AppIconLoader::AppIconLoader(Profile* profile,
                             int icon_size_in_dip,
                             AppIconLoaderDelegate* delegate)
    : profile_(profile),
      icon_size_in_dip_(icon_size_in_dip),
      delegate_(delegate) {}

AppIconLoader::~AppIconLoader() {}
