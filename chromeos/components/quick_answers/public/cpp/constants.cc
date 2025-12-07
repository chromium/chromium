// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/public/cpp/constants.h"

#include "ui/views/metadata/type_conversion.h"

DEFINE_ENUM_CONVERTERS(quick_answers::Intent,
                       {quick_answers::Intent::kDefinition, u"DEFINITION"},
                       {quick_answers::Intent::kTranslation, u"TRANSLATION"},
                       {quick_answers::Intent::kUnitConversion,
                        u"UNIT_CONVERSION"})
