// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SKILLS_PUBLIC_SKILLS_CONSTANTS_H_
#define COMPONENTS_SKILLS_PUBLIC_SKILLS_CONSTANTS_H_

namespace skills {

// The category assigned to skills provided by the user's organization (i.e.
// enterprise skills). This is a stable, unlocalized identifier: the WebUI
// matches against this exact string and localizes it on its own, so it must
// not be translated or reformatted here.
inline constexpr char kFromYourOrganizationCategory[] =
    "From your organization";

}  // namespace skills

#endif  // COMPONENTS_SKILLS_PUBLIC_SKILLS_CONSTANTS_H_
