// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_observer.h"

#include <memory>
#include <optional>
#include <string>

#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_model.h"
#include "chrome/browser/ui/views/page_action/page_action_model_observer.h"

namespace {
page_actions::PageActionState ModelToState(
    actions::ActionId action_id,
    const page_actions::PageActionModelInterface& model) {
  page_actions::PageActionState state;
  state.action_id = action_id;
  state.showing = model.GetVisible();
  state.chip_showing = model.IsChipShowing() && model.GetVisible();
  state.tooltip = model.GetVisible()
                      ? std::make_optional(model.GetTooltipText())
                      : std::nullopt;
  state.label = model.IsChipShowing() && model.GetVisible()
                    ? std::make_optional(model.GetText())
                    : std::nullopt;
  return state;
}
}  // namespace

namespace page_actions {

PageActionState::PageActionState() = default;
PageActionState::~PageActionState() = default;
PageActionState::PageActionState(const PageActionState&) = default;
PageActionState& PageActionState::operator=(const PageActionState&) = default;

bool PageActionState::operator==(const PageActionState& other) const {
  return action_id == other.action_id && showing == other.showing &&
         chip_showing == other.chip_showing && label == other.label &&
         tooltip == other.tooltip;
}

// The internal implementation of `PageActionObserver`, that observes a
// `PageActionModel` and distills it into a public-facing type.
class PageActionObserverImpl : PageActionModelObserver {
 public:
  PageActionObserverImpl(PageActionObserver& base, actions::ActionId action_id);
  ~PageActionObserverImpl() override;
  PageActionObserverImpl(const PageActionObserverImpl&) = delete;
  PageActionObserverImpl& operator=(const PageActionObserverImpl&) = delete;

  void RegisterAsPageActionObserver(PageActionController& controller);

  // PageActionModelObserver
  void OnPageActionModelChanged(const PageActionModelInterface& model) override;
  void OnPageActionModelWillBeDeleted(
      const PageActionModelInterface& model) override;

  const PageActionState& page_action() const { return page_action_; }

 private:
  const raw_ref<PageActionObserver> base_;
  base::ScopedObservation<PageActionModelInterface, PageActionModelObserver>
      observation_{this};

  const actions::ActionId action_id_;
  PageActionState page_action_;
};

PageActionObserverImpl::PageActionObserverImpl(PageActionObserver& base,
                                               actions::ActionId action_id)
    : base_(base), action_id_(action_id) {}

PageActionObserverImpl::~PageActionObserverImpl() = default;

void PageActionObserverImpl::RegisterAsPageActionObserver(
    PageActionController& controller) {
  controller.AddObserver(action_id_, observation_);
  page_action_ = ModelToState(action_id_, *observation_.GetSource());
}

void PageActionObserverImpl::OnPageActionModelChanged(
    const PageActionModelInterface& model) {
  PageActionState new_page_action_state = ModelToState(action_id_, model);

  if (new_page_action_state.showing && !page_action_.showing) {
    base_->OnPageActionIconShown(new_page_action_state);
  } else if (!new_page_action_state.showing && page_action_.showing) {
    base_->OnPageActionIconHidden(new_page_action_state);
  }

  if (new_page_action_state.chip_showing && !page_action_.chip_showing) {
    base_->OnPageActionChipShown(new_page_action_state);
  } else if (!new_page_action_state.chip_showing && page_action_.chip_showing) {
    base_->OnPageActionChipHidden(new_page_action_state);
  }

  page_action_ = new_page_action_state;
}

void PageActionObserverImpl::OnPageActionModelWillBeDeleted(
    const PageActionModelInterface& model) {
  observation_.Reset();
}

PageActionObserver::PageActionObserver(actions::ActionId action_id)
    : action_id_(action_id) {}

PageActionObserver::~PageActionObserver() = default;

void PageActionObserver::RegisterAsPageActionObserver(
    PageActionController& controller) {
  observer_impl_ = std::make_unique<PageActionObserverImpl>(*this, action_id_);
  observer_impl_->RegisterAsPageActionObserver(controller);
}

const PageActionState& PageActionObserver::GetCurrentPageActionState() const {
  return observer_impl_->page_action();
}

}  // namespace page_actions
