// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_UPDATER_UPDATER_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_UPDATER_UPDATER_PAGE_HANDLER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/ui/webui/updater/updater_ui.mojom.h"
#include "chrome/updater/mojom/updater_service.mojom.h"
#include "chrome/updater/updater_scope.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;

class UpdaterPageHandler final : public updater_ui::mojom::PageHandler {
 public:
  class Delegate : public base::RefCountedThreadSafe<Delegate> {
   public:
    static scoped_refptr<Delegate> CreateDefault();

    virtual std::optional<base::FilePath> GetUpdaterInstallDirectory(
        updater::UpdaterScope scope) const = 0;
    virtual std::optional<base::FilePath>
    GetEnterpriseCompanionInstallDirectory() const = 0;
    virtual void GetSystemUpdaterState(
        base::OnceCallback<void(const updater::mojom::UpdaterState&)> callback)
        const = 0;
    virtual void GetUserUpdaterState(
        base::OnceCallback<void(const updater::mojom::UpdaterState&)> callback)
        const = 0;
    virtual void GetSystemPoliciesJson(
        base::OnceCallback<void(const std::string&)> callback) const = 0;
    virtual void GetUserPoliciesJson(
        base::OnceCallback<void(const std::string&)> callback) const = 0;
    virtual void GetSystemUpdaterAppStates(
        base::OnceCallback<void(const std::vector<updater::mojom::AppState>&)>
            callback) const = 0;
    virtual void GetUserUpdaterAppStates(
        base::OnceCallback<void(const std::vector<updater::mojom::AppState>&)>
            callback) const = 0;

   protected:
    friend class base::RefCountedThreadSafe<Delegate>;

    virtual ~Delegate() = default;
  };

  UpdaterPageHandler(
      Profile* profile,
      mojo::PendingReceiver<updater_ui::mojom::PageHandler> receiver,
      mojo::PendingRemote<updater_ui::mojom::Page> page,
      scoped_refptr<Delegate> delegate = Delegate::CreateDefault());

  UpdaterPageHandler(const UpdaterPageHandler&) = delete;
  UpdaterPageHandler& operator=(const UpdaterPageHandler&) = delete;

  ~UpdaterPageHandler() override;

  void GetAllUpdaterEvents(GetAllUpdaterEventsCallback callback) override;
  void GetUpdaterStates(GetUpdaterStatesCallback callback) override;
  void GetEnterpriseCompanionState(
      GetEnterpriseCompanionStateCallback callback) override;
  void GetAppStates(GetAppStatesCallback callback) override;
  void ShowDirectory(updater_ui::mojom::ShowDirectoryTarget scope) override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);
  const raw_ptr<Profile> profile_;
  mojo::Receiver<updater_ui::mojom::PageHandler> receiver_;
  mojo::Remote<updater_ui::mojom::Page> page_;
  scoped_refptr<Delegate> delegate_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_UPDATER_UPDATER_PAGE_HANDLER_H_
