// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_CLIENT_IMPL_H_
#define CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_CLIENT_IMPL_H_

#include <vector>

#include "ash/public/cpp/holding_space/holding_space_client.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

class Profile;

namespace ash {

// Implementation of the holding space browser client.
class HoldingSpaceClientImpl : public HoldingSpaceClient {
 public:
  explicit HoldingSpaceClientImpl(Profile* profile);
  HoldingSpaceClientImpl(const HoldingSpaceClientImpl& other) = delete;
  HoldingSpaceClientImpl& operator=(const HoldingSpaceClientImpl& other) =
      delete;
  ~HoldingSpaceClientImpl() override;

  // HoldingSpaceClient:
  const std::string& AddItemOfType(HoldingSpaceItem::Type type,
                                   const base::FilePath& file_path) override;
  void CopyImageToClipboard(const HoldingSpaceItem&,
                            holding_space_metrics::EventSource event_source,
                            SuccessCallback) override;
  base::FilePath CrackFileSystemUrl(const GURL& file_system_url) const override;
  bool IsDriveDisabled() const override;
  void OpenDownloads(SuccessCallback callback) override;
  void OpenItems(const std::vector<const HoldingSpaceItem*>& items,
                 holding_space_metrics::EventSource event_source,
                 SuccessCallback callback) override;
  void OpenMyFiles(SuccessCallback callback) override;
  void PinFiles(const std::vector<base::FilePath>& file_paths,
                holding_space_metrics::EventSource event_source) override;
  void PinItems(const std::vector<const HoldingSpaceItem*>& items,
                holding_space_metrics::EventSource event_source) override;
  void RefreshSuggestions() override;
  void RemoveSuggestions(
      const std::vector<base::FilePath>& absolute_file_paths) override;
  void ShowItemInFolder(const HoldingSpaceItem& item,
                        holding_space_metrics::EventSource event_source,
                        SuccessCallback) override;
  void UnpinItems(const std::vector<const HoldingSpaceItem*>& items,
                  holding_space_metrics::EventSource event_source) override;

 private:
  const raw_ptr<Profile> profile_;

  base::WeakPtrFactory<HoldingSpaceClientImpl> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_CLIENT_IMPL_H_
