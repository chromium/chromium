// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/converters/tab_converters.h"

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/alert/tab_alert_controller.h"
#include "chrome/browser/ui/tabs/tab_network_state.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/browser_apis/tab_strip/tab_strip_api.mojom.h"
#include "components/browser_apis/tab_strip/tab_strip_api_data_model.mojom-shared.h"
#include "components/browser_apis/tab_strip/types/node_id.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/color/color_provider.h"
#include "url/gurl.h"

namespace tabs_api::converters {
namespace {

class FakeTabCollection : public tabs::TabCollection {
 public:
  explicit FakeTabCollection(Type type) : TabCollection(type, {}, true) {}
  ~FakeTabCollection() override = default;
};

using TabStripServiceConvertersBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(TabStripServiceConvertersBrowserTest, ConvertTab) {
  // Use a real browser and tab for testing.
  ASSERT_TRUE(AddTabAtIndex(0, GURL("chrome://newtab"),
                            ui::PageTransition::PAGE_TRANSITION_LINK));

  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);

  TabUIHelper* const tab_ui_helper = TabUIHelper::From(tab);
  ASSERT_TRUE(tab_ui_helper);

  const ui::ColorProvider& color_provider =
      tab->GetContents()->GetColorProvider();

  // Simulate setting some data that TabUIHelper would provide
  tab_ui_helper->SetNeedsAttention(true);
  auto mojo = BuildMojoTab(tab, color_provider,
                           {
                               .is_active = true,
                               .is_selected = true,
                           });

  ASSERT_EQ(base::NumberToString(tab->GetHandle().raw_value()), mojo->id.Id());
  ASSERT_EQ(NodeId::Type::kContent, mojo->id.Type());
  ASSERT_EQ(tab_ui_helper->GetVisibleURL(), mojo->url);
  ASSERT_EQ(base::UTF16ToUTF8(tab_ui_helper->GetTitle()), mojo->title);
  ASSERT_TRUE(mojo->is_active);
  ASSERT_TRUE(mojo->is_selected);
  ASSERT_EQ(TabNetworkStateForWebContents(tab->GetContents()),
            FromMojo(mojo->network_state));

  std::vector<mojom::AlertState> tab_alerts = mojo->alert_states;
  std::vector<tabs::TabAlert> mojom_tab_alerts = {};
  mojom_tab_alerts.reserve(tab_alerts.size());

  for (auto state : tab_alerts) {
    mojom_tab_alerts.push_back(FromMojo(state));
  }

  ASSERT_EQ(tabs::TabAlertController::From(tab)->GetAllActiveAlerts(),
            mojom_tab_alerts);
  ASSERT_EQ(tab->IsBlocked(), mojo->is_blocked);
}

IN_PROC_BROWSER_TEST_F(TabStripServiceConvertersBrowserTest,
                       ConvertTabCollection) {
  FakeTabCollection collection(tabs::TabCollection::Type::TABSTRIP);
  const std::string expected_id =
      base::NumberToString(collection.GetHandle().raw_value());
  auto mojo = BuildMojoTabCollectionData(collection.GetHandle());

  ASSERT_TRUE(mojo->is_tab_strip());

  const auto& tab_strip = mojo->get_tab_strip();
  ASSERT_EQ(expected_id, tab_strip->id.Id());
  ASSERT_EQ(NodeId::Type::kCollection, tab_strip->id.Type());
}

}  // namespace
}  // namespace tabs_api::converters
