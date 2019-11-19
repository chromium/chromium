// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/assets_loader.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/memory/singleton.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/vr/metrics/metrics_helper.h"
#include "chrome/browser/vr/model/assets.h"
#include "chrome/browser/vr/vr_buildflags.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "media/audio/wav_audio_handler.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"

namespace vr {

namespace {

constexpr char kMinVersionWithGradients[] = "1.1";
constexpr char kMinVersionWithSounds[] = "2.0";
constexpr char kMinVersionWithInactiveButtonClickSound[] = "2.2";

static const base::FilePath::CharType kBackgroundBaseFilename[] =
    FILE_PATH_LITERAL("background");
static const base::FilePath::CharType kNormalGradientBaseFilename[] =
    FILE_PATH_LITERAL("normal_gradient");
static const base::FilePath::CharType kIncognitoGradientBaseFilename[] =
    FILE_PATH_LITERAL("incognito_gradient");
static const base::FilePath::CharType kFullscreenGradientBaseFilename[] =
    FILE_PATH_LITERAL("fullscreen_gradient");
static const base::FilePath::CharType kPngExtension[] =
    FILE_PATH_LITERAL("png");
static const base::FilePath::CharType kJpegExtension[] =
    FILE_PATH_LITERAL("jpeg");

static const base::FilePath::CharType kButtonHoverSoundFilename[] =
    FILE_PATH_LITERAL("button_hover.wav");
static const base::FilePath::CharType kButtonClickSoundFilename[] =
    FILE_PATH_LITERAL("button_click.wav");
static const base::FilePath::CharType kBackButtonClickSoundFilename[] =
    FILE_PATH_LITERAL("back_button_click.wav");
static const base::FilePath::CharType kInactiveButtonClickSoundFilename[] =
    FILE_PATH_LITERAL("inactive_button_click.wav");

}  // namespace

struct AssetsLoaderSingletonTrait
    : public base::DefaultSingletonTraits<AssetsLoader> {
  static AssetsLoader* New() { return new AssetsLoader(); }
  static void Delete(AssetsLoader* assets) { delete assets; }
};

// static
AssetsLoader* AssetsLoader::GetInstance() {
  return base::Singleton<AssetsLoader, AssetsLoaderSingletonTrait>::get();
}

// static
base::Version AssetsLoader::MinVersionWithGradients() {
  return base::Version(kMinVersionWithGradients);
}

// static
bool AssetsLoader::AssetsSupported() {
#if BUILDFLAG(USE_VR_ASSETS_COMPONENT)
  return true;
#else   // BUILDFLAG(USE_VR_ASSETS_COMPONENT)
  return false;
#endif  // BUILDFLAG(USE_VR_ASSETS_COMPONENT)
}

void AssetsLoader::OnComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    std::unique_ptr<base::DictionaryValue> manifest) {
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AssetsLoader::OnComponentReadyInternal,
                     weak_ptr_factory_.GetWeakPtr(), version, install_dir));
}

void AssetsLoader::Load(OnAssetsLoadedCallback on_loaded) {
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AssetsLoader::LoadInternal,
                                weak_ptr_factory_.GetWeakPtr(),
                                base::ThreadTaskRunnerHandle::Get(),
                                std::move(on_loaded)));
}

MetricsHelper* AssetsLoader::GetMetricsHelper() {
  // If we instantiate metrics_helper_ in the constructor all functions of
  // MetricsHelper must be called in a valid sequence from the thread the
  // constructor ran on. However, the assets class can be instantiated from any
  // thread. To avoid the aforementioned restriction, create metrics_helper_ the
  // first time it is used and, thus, give the caller control over when the
  // sequence starts.
  if (!metrics_helper_) {
    metrics_helper_ = std::make_unique<MetricsHelper>();
  }
  return metrics_helper_.get();
}

bool AssetsLoader::ComponentReady() {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());
  return component_ready_;
}

void AssetsLoader::SetOnComponentReadyCallback(
    const base::RepeatingCallback<void()>& on_component_ready) {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());
  on_component_ready_callback_ = on_component_ready;
}

AssetsLoadStatus LoadImage(const base::FilePath& component_install_dir,
                           const base::FilePath::CharType* base_file,
                           std::unique_ptr<SkBitmap>* out_image) {
  bool is_png = false;
  std::string encoded_file_content;

  base::FilePath file_path = component_install_dir.Append(base_file);

  if (base::PathExists(file_path.AddExtension(kPngExtension))) {
    file_path = file_path.AddExtension(kPngExtension);
    is_png = true;
  } else if (base::PathExists(file_path.AddExtension(kJpegExtension))) {
    file_path = file_path.AddExtension(kJpegExtension);
  } else {
    return AssetsLoadStatus::kNotFound;
  }

  if (!base::ReadFileToString(file_path, &encoded_file_content)) {
    return AssetsLoadStatus::kParseFailure;
  }

  if (is_png) {
    (*out_image) = std::make_unique<SkBitmap>();
    if (!gfx::PNGCodec::Decode(
            reinterpret_cast<const unsigned char*>(encoded_file_content.data()),
            encoded_file_content.size(), out_image->get())) {
      out_image->reset();
    }
  } else {
    (*out_image) = gfx::JPEGCodec::Decode(
        reinterpret_cast<const unsigned char*>(encoded_file_content.data()),
        encoded_file_content.size());
  }

  if (!out_image->get()) {
    return AssetsLoadStatus::kInvalidContent;
  }

  return AssetsLoadStatus::kSuccess;
}

AssetsLoadStatus LoadSound(const base::FilePath& component_install_dir,
                           const base::FilePath::CharType* file_name,
                           std::unique_ptr<std::string>* out_buffer) {
  base::FilePath file_path = component_install_dir.Append(file_name);
  if (!base::PathExists(file_path)) {
    return AssetsLoadStatus::kNotFound;
  }

  auto buffer = std::make_unique<std::string>();
  if (!base::ReadFileToString(file_path, buffer.get())) {
    return AssetsLoadStatus::kParseFailure;
  }

  if (!media::WavAudioHandler::Create(*buffer)) {
    return AssetsLoadStatus::kInvalidContent;
  }

  *out_buffer = std::move(buffer);
  return AssetsLoadStatus::kSuccess;
}

// static
void AssetsLoader::LoadAssetsTask(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const base::Version& component_version,
    const base::FilePath& component_install_dir,
    OnAssetsLoadedCallback on_loaded) {
  auto assets = std::make_unique<Assets>();
  AssetsLoadStatus status = AssetsLoadStatus::kSuccess;

  status = LoadImage(component_install_dir, kBackgroundBaseFilename,
                     &assets->background);

  if (component_version >= AssetsLoader::MinVersionWithGradients()) {
    if (status == AssetsLoadStatus::kSuccess) {
      status = LoadImage(component_install_dir, kNormalGradientBaseFilename,
                         &assets->normal_gradient);
    }
    if (status == AssetsLoadStatus::kSuccess) {
      status = LoadImage(component_install_dir, kIncognitoGradientBaseFilename,
                         &assets->incognito_gradient);
    }
    if (status == AssetsLoadStatus::kSuccess) {
      status = LoadImage(component_install_dir, kFullscreenGradientBaseFilename,
                         &assets->fullscreen_gradient);
    }
  }

  std::vector<std::tuple<const char*, const base::FilePath::CharType*,
                         std::unique_ptr<std::string>*>>
      sounds = {{kMinVersionWithSounds, kButtonHoverSoundFilename,
                 &assets->button_hover_sound},
                {kMinVersionWithSounds, kButtonClickSoundFilename,
                 &assets->button_click_sound},
                {kMinVersionWithSounds, kBackButtonClickSoundFilename,
                 &assets->back_button_click_sound},
                {kMinVersionWithInactiveButtonClickSound,
                 kInactiveButtonClickSoundFilename,
                 &assets->inactive_button_click_sound}};

  auto sounds_iter = sounds.begin();
  while (status == AssetsLoadStatus::kSuccess && sounds_iter != sounds.end()) {
    const char* min_version;
    const base::FilePath::CharType* file_name;
    std::unique_ptr<std::string>* data;
    std::tie(min_version, file_name, data) = *sounds_iter;
    if (component_version >= base::Version(min_version)) {
      status = LoadSound(component_install_dir, file_name, data);
    }
    sounds_iter++;
  }

  if (status != AssetsLoadStatus::kSuccess) {
    assets.reset();
  }

  task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(on_loaded), status, std::move(assets),
                                component_version));
}

AssetsLoader::AssetsLoader()
    : main_thread_task_runner_(
          base::CreateSingleThreadTaskRunner({content::BrowserThread::UI})) {
  DCHECK(main_thread_task_runner_.get());
}

AssetsLoader::~AssetsLoader() = default;

void AssetsLoader::OnComponentReadyInternal(const base::Version& version,
                                            const base::FilePath& install_dir) {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());
  component_version_ = version;
  component_install_dir_ = install_dir;
  component_ready_ = true;
  if (on_component_ready_callback_) {
    on_component_ready_callback_.Run();
  }
  GetMetricsHelper()->OnComponentReady(version);
}

void AssetsLoader::LoadInternal(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    OnAssetsLoadedCallback on_loaded) {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());
  DCHECK(component_ready_);
  base::PostTask(
      FROM_HERE,
      {base::ThreadPool(), base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(&AssetsLoader::LoadAssetsTask, task_runner,
                     component_version_, component_install_dir_,
                     std::move(on_loaded)));
}

}  // namespace vr
