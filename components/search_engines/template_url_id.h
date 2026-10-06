// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_ID_H_
#define COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_ID_H_

#include "base/types/id_type.h"

class TemplateURL;

// ID of a search provider.
using TemplateURLID = base::IdType64<TemplateURL>;

inline constexpr TemplateURLID kInvalidTemplateURLID;

#endif  // COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_ID_H_
