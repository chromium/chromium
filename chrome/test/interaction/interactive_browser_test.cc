// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/interaction/interactive_browser_test.h"

#include <utility>

#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/webui/tracked_element/tracked_element_handler.h"
#include "ui/webui/tracked_element/tracked_element_web_ui.h"

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

InteractiveBrowserTestApi::StepBuilder
InteractiveBrowserTestApi::InstrumentWebContentsContaining(
    ui::ElementIdentifier id,
    ElementSpecifier webui_element) {
  return AfterShow(
             webui_element,
             base::BindLambdaForTesting([this, id](ui::InteractionSequence* seq,
                                                   ui::TrackedElement* el) {
               auto* const web_el = el->AsA<ui::TrackedElementWebUI>();
               if (!web_el) {
                 LOG(ERROR) << "Element " << *el << " is not a WebUI element.";
                 seq->FailForTesting();
                 return;
               }
               auto* const tab = tabs::TabModel::MaybeGetFromContents(
                   web_el->handler()->web_contents());
               if (tab) {
                 auto* const browser = tab->GetBrowserWindowInterface();
                 int index = browser->tab_strip_model()->GetIndexOfTab(tab);
                 CHECK_NE(TabStripModel::kNoTab, index);
                 browser_test_impl().AddInstrumentedWebContents(
                     WebContentsInteractionTestUtil::ForExistingTabInBrowser(
                         browser, id, index));
               } else {
                 auto* const view = web_el->GetWebView();
                 if (!view) {
                   LOG(ERROR) << "Element " << *el << " has no web view.";
                   seq->FailForTesting();
                   return;
                 }
                 browser_test_impl().AddInstrumentedWebContents(
                     WebContentsInteractionTestUtil::ForNonTabWebView(view,
                                                                      id));
               }
             }))
      .AddDescriptionPrefix(base::StringPrintf(
          "InstrumentNonTabWebViewContaining( %s )", id.GetName().c_str()));
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
