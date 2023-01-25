// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_CROSAPI_NEW_WINDOW_DELEGATE_H_
#define CHROME_BROWSER_UI_ASH_CROSAPI_NEW_WINDOW_DELEGATE_H_

#include "ash/public/cpp/new_window_delegate.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "components/exo/wm_helper.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"

// Handles opening new tabs and windows on behalf on ash.
// Use crosapi to control Lacros Browser.
// Web browser unrelated operations are forwarded to the given delegate.
class CrosapiNewWindowDelegate : public ash::NewWindowDelegate {
 public:
  // CrosapiNewWindowDelegate forwards methods which are not related to
  // web browser to the given |delegate|.
  explicit CrosapiNewWindowDelegate(ash::NewWindowDelegate* delegate);
  CrosapiNewWindowDelegate(const CrosapiNewWindowDelegate&) = delete;
  const CrosapiNewWindowDelegate& operator=(const CrosapiNewWindowDelegate&) =
      delete;
  ~CrosapiNewWindowDelegate() override;

  // Overridden from ash::NewWindowDelegate:
  void NewTab() override;
  void NewWindow(bool incognito, bool should_trigger_session_restore) override;
  void NewWindowForDetachingTab(
      aura::Window* source_window,
      const ui::OSExchangeData& drop_data,
      NewWindowForDetachingTabCallback closure) override;
  void OpenUrl(const GURL& url,
               OpenUrlFrom from,
               Disposition disposition) override;
  void OpenCalculator() override;
  void OpenFileManager() override;
  void OpenDownloadsFolder() override;
  void OpenCrosh() override;
  void OpenGetHelp() override;
  void RestoreTab() override;
  void ShowKeyboardShortcutViewer() override;
  void ShowShortcutCustomizationApp() override;
  void ShowTaskManager() override;
  void OpenDiagnostics() override;
  void OpenFeedbackPage(FeedbackSource source,
                        const std::string& description_template) override;
  void OpenPersonalizationHub() override;

 private:
  // Observes the aura::Window instances created after the webui tab-drop
  // has been requested. Its OnWindowPropertyChanged() override checks for
  // a specific |window_id| to invoke the closure routine.
  class WindowObserver : public exo::WMHelper::ExoWindowObserver,
                         public aura::WindowObserver {
   public:
    explicit WindowObserver(CrosapiNewWindowDelegate* owner,
                            NewWindowForDetachingTabCallback closure);
    ~WindowObserver() override;

    // aura::WindowObserver overrides.
    void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;

    // WMHelper::ExoWindowObserver overrides.
    void OnExoWindowCreated(aura::Window* window) override;

    void SetWindowID(const std::string& window_id);

   private:
    CrosapiNewWindowDelegate* owner_;

    // Observes windows launched after window tab-drop request.
    base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
        observed_windows_{this};

    // The callback must be called, even out of the scope of
    // OnWindowPropertyChanged(), since it owns ownership of its callee
    // (ash::TabDragDropDelegate).
    NewWindowForDetachingTabCallback closure_;

    // Stores the window id of the Lacros window created by
    // CrosapiNewWindowDelegate::NewWindowForDetachingTab().
    std::string window_id_;

    // Stores the set of Exo aura::Window instances whose surface widget has
    // been committed prior to |window_id_| is set.
    std::set<aura::Window*> windows_committed_prior_to_window_id_;
  };

  // Destroys the WindowObserver once the "WebUI tab-drop closure routine has
  // been invoked".
  void DestroyWindowObserver();

  // Not owned. Practically, this should point to ChromeNewWindowClient in
  // production.
  ash::NewWindowDelegate* const delegate_;

  std::unique_ptr<WindowObserver> window_observer_;
};

#endif  // CHROME_BROWSER_UI_ASH_CROSAPI_NEW_WINDOW_DELEGATE_H_
