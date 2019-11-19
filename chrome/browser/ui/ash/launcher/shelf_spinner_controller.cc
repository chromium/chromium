// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/shelf_spinner_controller.h"

#include <vector>

#include "ash/public/cpp/shelf_model.h"
#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/shelf_spinner_item_controller.h"
#include "components/user_manager/user_manager.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_throbber.h"

namespace {

constexpr int kUpdateIconIntervalMs = 40;  // 40ms for 25 frames per second.

// Controls the spinner animation. See crbug.com/922977 for details.
constexpr base::TimeDelta kFadeInDuration =
    base::TimeDelta::FromMilliseconds(200);
constexpr base::TimeDelta kFadeOutDuration =
    base::TimeDelta::FromMilliseconds(200);
constexpr base::TimeDelta kMinimumShowDuration =
    base::TimeDelta::FromMilliseconds(200);

constexpr int kSpinningGapPercent = 25;
constexpr color_utils::HSL kInactiveHslShift = {-1, 0, 0.25};
constexpr double kInactiveTransparency = 0.5;

// Returns the proportion of the duration |d| from |t1| to |t2|, where 0 means
// |t2| is before or at |t1| and 1 means it is |d| or further ahead.
double TimeProportionSince(const base::Time& t1,
                           const base::Time& t2,
                           const base::TimeDelta& d) {
  return std::max(
      0.0, std::min(1.0, (t2 - t1).InMillisecondsF() / d.InMillisecondsF()));
}

}  // namespace

class ShelfSpinnerController::ShelfSpinnerData {
 public:
  explicit ShelfSpinnerData(ShelfSpinnerItemController* controller)
      : controller_(controller),
        creation_time_(controller->start_time()),
        removal_time_() {}

  ~ShelfSpinnerData() = default;

  // Returns true if we are currently fading the spinner in. This will also
  // return true when the spinner is animating but has finished fading in.
  bool IsFadingIn() const {
    return !IsKilled() || (base::Time::Now() < removal_time_);
  }

  // Returns true if we have completed the fade-out animation.
  bool IsFinished() const {
    return IsKilled() && base::Time::Now() >= removal_time_ + kFadeOutDuration;
  }

  // Returns true if this spinner has been killed (no matter what stage of the
  // animation it is up to).
  bool IsKilled() const { return controller_ == nullptr; }

  // Marks the spinner as completed, which begins the fade out animation
  // either now, or at a point in the future when the minimum show duration
  // has been met.
  void Kill() {
    removal_time_ =
        std::max(base::Time::Now(), creation_time_ + kMinimumShowDuration);
    controller_ = nullptr;
  }

  ShelfSpinnerItemController* controller() const { return controller_; }

  // Get a timestamp for when the spinner was started.
  base::Time creation_time() const { return creation_time_; }

  // Get a timestamp for when the spinner's fade-out animation begins. This
  // will be in the future if the spiiner was Kill()ed before the minimum show
  // duration was reached.
  base::Time removal_time() const { return removal_time_; }

 private:
  ShelfSpinnerItemController* controller_;
  base::Time creation_time_;
  base::Time removal_time_;
};

namespace {

class SpinningEffectSource : public gfx::CanvasImageSource {
 public:
  SpinningEffectSource(ShelfSpinnerController::ShelfSpinnerData data,
                       const gfx::ImageSkia& image,
                       bool is_pinned)
      : gfx::CanvasImageSource(image.size()),
        data_(std::move(data)),
        active_image_(
            (is_pinned || !data_.IsFadingIn())
                ? image
                : gfx::ImageSkiaOperations::CreateTransparentImage(image, 0)),
        inactive_image_(gfx::ImageSkiaOperations::CreateTransparentImage(
            gfx::ImageSkiaOperations::CreateHSLShiftedImage(image,
                                                            kInactiveHslShift),
            kInactiveTransparency)) {}

  ~SpinningEffectSource() override {}

  // gfx::CanvasImageSource override.
  void Draw(gfx::Canvas* canvas) override {
    base::Time now = base::Time::Now();
    double animation_lirp = GetAnimationStage(now);

    canvas->DrawImageInt(gfx::ImageSkiaOperations::CreateBlendedImage(
                             inactive_image_, active_image_, animation_lirp),
                         0, 0);

    const int gap = kSpinningGapPercent * inactive_image_.width() / 100;
    gfx::PaintThrobberSpinning(
        canvas,
        gfx::Rect(gap, gap, inactive_image_.width() - 2 * gap,
                  inactive_image_.height() - 2 * gap),
        SkColorSetA(SK_ColorWHITE, 0xFF * (1.0 - std::abs(animation_lirp))),
        now - data_.creation_time());
  }

 private:
  // Returns a number in the range [0, 1] where:
  //  - 0   -> spinner image is completely shown.
  //  - 0.5 -> spinner image is half-way gone.
  //  - 1   -> normal image is shown.
  double GetAnimationStage(const base::Time& now) {
    if (data_.IsFadingIn()) {
      return 1.0 -
             TimeProportionSince(data_.creation_time(), now, kFadeInDuration);
    } else {
      return TimeProportionSince(data_.removal_time(), now, kFadeOutDuration);
    }
  }

  ShelfSpinnerController::ShelfSpinnerData data_;
  const gfx::ImageSkia active_image_;
  const gfx::ImageSkia inactive_image_;

  DISALLOW_COPY_AND_ASSIGN(SpinningEffectSource);
};

}  // namespace

ShelfSpinnerController::ShelfSpinnerController(ChromeLauncherController* owner)
    : owner_(owner) {
  owner->shelf_model()->AddObserver(this);
  if (user_manager::UserManager::IsInitialized()) {
    if (auto* active_user = user_manager::UserManager::Get()->GetActiveUser())
      current_account_id_ = active_user->GetAccountId();
    else
      LOG(ERROR) << "Failed to get active user, UserManager returned null";
  } else {
    LOG(ERROR) << "Failed to get active user, UserManager is not initialized";
  }
}

ShelfSpinnerController::~ShelfSpinnerController() {
  owner_->shelf_model()->RemoveObserver(this);
}

void ShelfSpinnerController::MaybeApplySpinningEffect(const std::string& app_id,
                                                      gfx::ImageSkia* image) {
  DCHECK(image);
  auto it = app_controller_map_.find(app_id);
  if (it == app_controller_map_.end())
    return;

  *image = gfx::ImageSkia(std::make_unique<SpinningEffectSource>(
                              it->second, *image, owner_->IsAppPinned(app_id)),
                          image->size());
}

void ShelfSpinnerController::HideSpinner(const std::string& app_id) {
  if (!RemoveSpinnerFromControllerMap(app_id))
    return;

  const ash::ShelfID shelf_id(app_id);

  // If the app whose spinner is being hidden is pinned, we don't want to un-pin
  // it when we remove it from the shelf, so disable pin syncing while we update
  // things.
  auto pin_disabler = owner_->GetScopedPinSyncDisabler();
  // The static_cast here is safe, because if the delegate were not a
  // ShelfSpinnerItemController then ShelfItemDelegateChanged would have been
  // called and we would not have reached this place.
  auto delegate =
      owner_->shelf_model()->RemoveItemAndTakeShelfItemDelegate(shelf_id);
  std::unique_ptr<ShelfSpinnerItemController> cast_delegate(
      static_cast<ShelfSpinnerItemController*>(delegate.release()));

  hidden_app_controller_map_.emplace(
      current_account_id_, std::make_pair(app_id, std::move(cast_delegate)));
}

void ShelfSpinnerController::CloseSpinner(const std::string& app_id) {
  if (!RemoveSpinnerFromControllerMap(app_id))
    return;

  owner_->CloseLauncherItem(ash::ShelfID(app_id));
  UpdateShelfItemIcon(app_id);
}

bool ShelfSpinnerController::RemoveSpinnerFromControllerMap(
    const std::string& app_id) {
  AppControllerMap::const_iterator it = app_controller_map_.find(app_id);
  if (it == app_controller_map_.end())
    return false;

  const ash::ShelfID shelf_id(app_id);
  DCHECK_EQ(it->second.controller(),
            owner_->shelf_model()->GetShelfItemDelegate(shelf_id));
  app_controller_map_.erase(it);

  return true;
}

void ShelfSpinnerController::CloseCrostiniSpinners() {
  std::vector<std::string> app_ids_to_close;
  crostini::CrostiniRegistryService* registry_service =
      crostini::CrostiniRegistryServiceFactory::GetForProfile(
          chromeos::ProfileHelper::Get()->GetProfileByAccountId(
              current_account_id_));
  for (const auto& app_id_controller_pair : app_controller_map_) {
    if (registry_service->IsCrostiniShelfAppId(app_id_controller_pair.first))
      app_ids_to_close.push_back(app_id_controller_pair.first);
  }
  for (const auto& app_id : app_ids_to_close)
    CloseSpinner(app_id);
}

bool ShelfSpinnerController::HasApp(const std::string& app_id) const {
  auto it = app_controller_map_.find(app_id);
  return it != app_controller_map_.end() && !it->second.IsKilled();
}

base::TimeDelta ShelfSpinnerController::GetActiveTime(
    const std::string& app_id) const {
  AppControllerMap::const_iterator it = app_controller_map_.find(app_id);
  if (it == app_controller_map_.end())
    return base::TimeDelta();

  return base::Time::Now() - it->second.creation_time();
}

Profile* ShelfSpinnerController::OwnerProfile() {
  return owner_->profile();
}

void ShelfSpinnerController::ShelfItemDelegateChanged(
    const ash::ShelfID& id,
    ash::ShelfItemDelegate* old_delegate,
    ash::ShelfItemDelegate* delegate) {
  auto it = app_controller_map_.find(id.app_id);
  if (it != app_controller_map_.end()) {
    it->second.Kill();
  }
}

void ShelfSpinnerController::ActiveUserChanged(const AccountId& account_id) {
  if (account_id == current_account_id_) {
    LOG(WARNING) << "Tried switching to currently active user";
    return;
  }

  std::vector<std::string> to_hide;
  std::vector<
      std::pair<std::string, std::unique_ptr<ShelfSpinnerItemController>>>
      to_show;

  for (const auto& app_id : app_controller_map_)
    to_hide.push_back(app_id.first);
  for (auto it = hidden_app_controller_map_.lower_bound(account_id);
       it != hidden_app_controller_map_.upper_bound(account_id); it++) {
    to_show.push_back(std::move(it->second));
  }

  hidden_app_controller_map_.erase(
      hidden_app_controller_map_.lower_bound(account_id),
      hidden_app_controller_map_.upper_bound(account_id));

  for (const auto& app_id : to_hide)
    HideSpinner(app_id);

  for (auto& app_id_delegate_pair : to_show) {
    AddSpinnerToShelf(app_id_delegate_pair.first,
                      std::move(app_id_delegate_pair.second));
  }

  current_account_id_ = account_id;
}

void ShelfSpinnerController::UpdateShelfItemIcon(const std::string& app_id) {
  owner_->UpdateLauncherItemImage(app_id);
}

void ShelfSpinnerController::UpdateApps() {
  if (app_controller_map_.empty())
    return;

  RegisterNextUpdate();
  std::vector<std::string> app_ids_to_close;
  for (const auto& pair : app_controller_map_) {
    UpdateShelfItemIcon(pair.first);
    if (pair.second.IsFinished())
      app_ids_to_close.emplace_back(pair.first);
  }
  for (const auto& app_id : app_ids_to_close) {
    if (RemoveSpinnerFromControllerMap(app_id))
      UpdateShelfItemIcon(app_id);
  }
}

void ShelfSpinnerController::RegisterNextUpdate() {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ShelfSpinnerController::UpdateApps,
                     weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(kUpdateIconIntervalMs));
}

void ShelfSpinnerController::AddSpinnerToShelf(
    const std::string& app_id,
    std::unique_ptr<ShelfSpinnerItemController> controller) {
  const ash::ShelfID shelf_id(app_id);

  // We should only apply the spinner controller only over non-active items.
  const ash::ShelfItem* item = owner_->GetItem(shelf_id);
  if (item && item->status != ash::STATUS_CLOSED)
    return;

  controller->SetHost(weak_ptr_factory_.GetWeakPtr());
  ShelfSpinnerItemController* item_controller = controller.get();
  if (!item) {
    owner_->CreateAppLauncherItem(std::move(controller), ash::STATUS_RUNNING);
  } else {
    owner_->shelf_model()->SetShelfItemDelegate(shelf_id,
                                                std::move(controller));
    owner_->SetItemStatus(shelf_id, ash::STATUS_RUNNING);
  }

  if (app_controller_map_.empty())
    RegisterNextUpdate();

  app_controller_map_.emplace(app_id, ShelfSpinnerData(item_controller));
}
