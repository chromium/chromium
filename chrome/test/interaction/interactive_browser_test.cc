// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/interaction/interactive_browser_test.h"

#include <utility>

#include "ui/base/interaction/element_identifier.h"

InteractiveBrowserTestApi::MultiStep
InteractiveBrowserTestApi::InstrumentNonTabWebView(ui::ElementIdentifier id,
                                                   ElementSpecifier web_view,
                                                   bool wait_for_ready) {
  auto steps = Steps(AfterShow(
      web_view, base::BindLambdaForTesting([this, id](ui::TrackedElement* el) {
        browser_test_impl().AddInstrumentedWebContents(
            WebContentsInteractionTestUtil::ForNonTabWebView(
                AsView<views::WebView>(el), id));
      })));
  if (wait_for_ready) {
    steps.push_back(WaitForWebContentsReady(id));
  }
  AddDescriptionPrefix(
      steps, base::StringPrintf("InstrumentNonTabWebView( %s, %d, )",
                                id.GetName().c_str(), wait_for_ready));
  return steps;
}

InteractiveBrowserTestApi::MultiStep
InteractiveBrowserTestApi::InstrumentNonTabWebView(
    ui::ElementIdentifier id,
    AbsoluteViewSpecifier web_view,
    bool wait_for_ready) {
  static constexpr char kTemporaryElementName[] =
      "__InstrumentNonTabWebViewTemporaryElementName__";
  auto steps =
      Steps(NameView(kTemporaryElementName, std::move(web_view)),
            InstrumentNonTabWebView(id, kTemporaryElementName, wait_for_ready));
  AddDescriptionPrefix(steps, "InstrumentNonTabWebView()");
  return steps;
}

InteractiveBrowserTestApi::MultiStep InteractiveBrowserTestApi::MoveMouseTo(
    ui::ElementIdentifier web_contents,
    const DeepQuery& where) {
  auto steps = Steps(WaitForWebContentsPainted(web_contents),
                     InSameContext(MoveMouseTo(
                         web_contents, DeepQueryToRelativePosition(where))));
  AddDescriptionPrefix(steps, "MoveMouseTo()");
  return steps;
}

InteractiveBrowserTestApi::MultiStep InteractiveBrowserTestApi::DragMouseTo(
    ui::ElementIdentifier web_contents,
    const DeepQuery& where,
    bool release) {
  auto steps =
      Steps(WaitForWebContentsPainted(web_contents),
            InSameContext(DragMouseTo(
                web_contents, DeepQueryToRelativePosition(where), release)));
  AddDescriptionPrefix(steps, "DragMouseTo()");
  return steps;
}
