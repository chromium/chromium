// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ASSETS_LOADER_H_
#define CHROME_BROWSER_VR_ASSETS_LOADER_H_

#include <stdint.h>
#include <memory>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/version.h"
#include "chrome/browser/vr/assets_load_status.h"
#include "chrome/browser/vr/vr_base_export.h"

namespace base {
class DictionaryValue;
class SingleThreadTaskRunner;
class Version;
}  // namespace base

namespace vr {

// Major component version we need to support all features.
constexpr uint32_t kTargetMajorVrAssetsComponentVersion = 2;
// Minimum major component version we are able to use with potentially reduced
// set of features.
constexpr uint32_t kMinMajorVrAssetsComponentVersion = 1;

class MetricsHelper;
struct AssetsLoaderSingletonTrait;
struct Assets;

// Manages VR assets such as the environment. Gets updated by the VR assets
// component.
//
// If not noted otherwise the functions are thread-safe. The reason is that the
// component will be made available on a different thread than the asset load
// request. Internally, the function calls will be posted on the main thread.
// The asset load may be performed on a worker thread.
class VR_BASE_EXPORT AssetsLoader {
 public:
  typedef base::OnceCallback<void(AssetsLoadStatus status,
                                  std::unique_ptr<Assets> assets,
                                  const base::Version& component_version)>
      OnAssetsLoadedCallback;
  typedef base::RepeatingCallback<void()> OnComponentReadyCallback;

  // Returns the single assets instance and creates it on first call.
  static AssetsLoader* GetInstance();

  static base::Version MinVersionWithGradients();
  static bool AssetsSupported();

  // Tells VR assets that a new VR assets component version is ready for use.
  void OnComponentReady(const base::Version& version,
                        const base::FilePath& install_dir,
                        std::unique_ptr<base::DictionaryValue> manifest);

  // Loads asset files and calls |on_loaded| passing the loaded asset files.
  // |on_loaded| will be called on the caller's thread. Component must be ready
  // when calling this function.
  void Load(OnAssetsLoadedCallback on_loaded);

  MetricsHelper* GetMetricsHelper();

  // Returns true if the component is ready.
  // Must be called on the main thread.
  bool ComponentReady();

  // |on_component_ready| is called on main thread when assets component becomes
  // ready to use or got updated. Must be called on the main thread.
  void SetOnComponentReadyCallback(
      const OnComponentReadyCallback& on_component_ready);

 private:
  static void LoadAssetsTask(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      const base::Version& component_version,
      const base::FilePath& component_install_dir,
      OnAssetsLoadedCallback on_loaded);

  AssetsLoader();
  ~AssetsLoader();
  void OnComponentReadyInternal(const base::Version& version,
                                const base::FilePath& install_dir);
  void LoadInternal(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                    OnAssetsLoadedCallback on_loaded);

  bool component_ready_ = false;
  base::Version component_version_;
  base::FilePath component_install_dir_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  std::unique_ptr<MetricsHelper> metrics_helper_;
  OnComponentReadyCallback on_component_ready_callback_;

  base::WeakPtrFactory<AssetsLoader> weak_ptr_factory_{this};

  friend struct AssetsLoaderSingletonTrait;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ASSETS_LOADER_H_
