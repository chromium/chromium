// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INFOBARS_CORE_INFOBAR_CONTAINER_H_
#define COMPONENTS_INFOBARS_CORE_INFOBAR_CONTAINER_H_

#include <stddef.h>

#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "components/infobars/core/infobar_manager.h"
#include "third_party/skia/include/core/SkColor.h"

namespace infobars {

class InfoBar;

// InfoBarContainer is a cross-platform base class to handle the visibility-
// related aspects of InfoBars.  While InfoBarManager owns the InfoBars, the
// InfoBarContainer is responsible for telling particular InfoBars that they
// should be hidden or visible.
//
// Platforms need to subclass this to implement a few platform-specific
// functions, which are pure virtual here.
class InfoBarContainer : public InfoBarManager::Observer {
 public:
  class Delegate {
   public:
    // The delegate is notified each time the infobar container changes height,
    // as well as when it stops animating.
    virtual void InfoBarContainerStateChanged(bool is_animating) = 0;

   protected:
    virtual ~Delegate();
  };

  explicit InfoBarContainer(Delegate* delegate);
  ~InfoBarContainer() override;

  // Changes the InfoBarManager for which this container is showing infobars.
  // This will hide all current infobars, remove them from the container, add
  // the infobars from |infobar_manager|, and show them all.  |infobar_manager|
  // may be NULL.
  void ChangeInfoBarManager(InfoBarManager* infobar_manager);

  // Called when a contained infobar has animated or by some other means changed
  // its height, or when it stops animating.  The container is expected to do
  // anything necessary to respond, e.g. re-layout.
  void OnInfoBarStateChanged(bool is_animating);

  // Called by |infobar| to request that it be removed from the container.  At
  // this point, |infobar| should already be hidden.
  void RemoveInfoBar(InfoBar* infobar);

  const Delegate* delegate() const { return delegate_; }

 protected:
  // Subclasses must call this during destruction, so that we can remove
  // infobars (which will call the pure virtual functions below) while the
  // subclass portion of |this| has not yet been destroyed.
  void RemoveAllInfoBarsForDestruction();

  // These must be implemented on each platform to e.g. adjust the visible
  // object hierarchy.  The first two functions should each be called exactly
  // once during an infobar's life (see comments on RemoveInfoBar() and
  // AddInfoBar()).
  virtual void PlatformSpecificAddInfoBar(InfoBar* infobar,
                                          size_t position) = 0;
  // TODO(miguelg): Remove this; it is only necessary for Android, and only
  // until the translate infobar is implemented as three different infobars like
  // GTK does.
  virtual void PlatformSpecificReplaceInfoBar(InfoBar* old_infobar,
                                              InfoBar* new_infobar) {}
  virtual void PlatformSpecificRemoveInfoBar(InfoBar* infobar) = 0;
  virtual void PlatformSpecificInfoBarStateChanged(bool is_animating) {}

 private:
  typedef std::vector<InfoBar*> InfoBars;

  // InfoBarManager::Observer:
  void OnInfoBarAdded(InfoBar* infobar) override;
  void OnInfoBarRemoved(InfoBar* infobar, bool animate) override;
  void OnInfoBarReplaced(InfoBar* old_infobar, InfoBar* new_infobar) override;
  void OnManagerShuttingDown(InfoBarManager* manager) override;

  // Adds |infobar| to this container before the existing infobar at position
  // |position| and calls Show() on it.  |animate| is passed along to
  // infobar->Show().
  void AddInfoBar(InfoBar* infobar, size_t position, bool animate);

  Delegate* delegate_;
  InfoBarManager* infobar_manager_;
  InfoBars infobars_;

  // Normally false.  When true, OnInfoBarStateChanged() becomes a no-op.  We
  // use this to ensure that ChangeInfoBarManager() only executes the
  // functionality in OnInfoBarStateChanged() once, to minimize unnecessary
  // layout and painting.
  bool ignore_infobar_state_changed_;

  DISALLOW_COPY_AND_ASSIGN(InfoBarContainer);
};

}  // namespace infobars

#endif  // COMPONENTS_INFOBARS_CORE_INFOBAR_CONTAINER_H_
