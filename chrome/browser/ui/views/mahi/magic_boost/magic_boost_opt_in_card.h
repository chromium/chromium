// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MAHI_MAGIC_BOOST_MAGIC_BOOST_OPT_IN_CARD_H_
#define CHROME_BROWSER_UI_VIEWS_MAHI_MAGIC_BOOST_MAGIC_BOOST_OPT_IN_CARD_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/flex_layout_view.h"

namespace chromeos {

// The Magic Boost opt-in card view.
class MagicBoostOptInCard : public views::FlexLayoutView {
  METADATA_HEADER(MagicBoostOptInCard, views::FlexLayoutView)

 public:
  MagicBoostOptInCard();
  MagicBoostOptInCard(const MagicBoostOptInCard&) = delete;
  MagicBoostOptInCard& operator=(const MagicBoostOptInCard&) = delete;
  ~MagicBoostOptInCard() override;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_VIEWS_MAHI_MAGIC_BOOST_MAGIC_BOOST_OPT_IN_CARD_H_
