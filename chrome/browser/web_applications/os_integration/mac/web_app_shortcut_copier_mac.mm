// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Installs the native app shim for a web app to its final location.

#import <Foundation/Foundation.h>

#include <memory>

#include "base/apple/bundle_locations.h"
#include "base/apple/foundation_util.h"
#include "base/apple/mach_port_rendezvous.h"
#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/mac/process_requirement.h"
#include "base/message_loop/message_pump_type.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_shared_memory.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/single_thread_task_executor.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/web_applications/mojom/web_app_shortcut_copier.mojom.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_version.h"
#include "mojo/core/embedder/configuration.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/system/invitation.h"

namespace {

class WebAppShortcutCopierImpl : public web_app::mojom::WebAppShortcutCopier {
 public:
  explicit WebAppShortcutCopierImpl(
      mojo::PendingReceiver<web_app::mojom::WebAppShortcutCopier>
          pending_receiver,
      base::OnceClosure quit_callback)
      : receiver_(this, std::move(pending_receiver)),
        quit_callback_(std::move(quit_callback)) {}

  void CopyWebAppShortcut(const base::FilePath& source_path,
                          const base::FilePath& destination_path,
                          CopyWebAppShortcutCallback callback) override {
    if (base::CopyDirectory(source_path, destination_path, true)) {
      std::move(callback).Run(true);
    } else {
      LOG(ERROR) << "Copying app from " << source_path << " to "
                 << destination_path << " failed.";
      std::move(callback).Run(false);
    }

    std::move(quit_callback_).Run();
  }

 private:
  mojo::Receiver<web_app::mojom::WebAppShortcutCopier> receiver_;
  base::OnceClosure quit_callback_;
};

std::optional<base::mac::ProcessRequirement> CallerProcessRequirement() {
  return base::mac::ProcessRequirement::Builder()
      .SignedWithSameIdentity()
      .IdentifierIsOneOf({
          MAC_BUNDLE_IDENTIFIER_STRING,
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
          MAC_BUNDLE_IDENTIFIER_STRING ".beta",
          MAC_BUNDLE_IDENTIFIER_STRING ".canary",
          MAC_BUNDLE_IDENTIFIER_STRING ".dev",
#endif
      })
      // Chrome can be dynamically valid but not match its signature on disk
      // if an update is staged and waiting for the browser to be relaunched.
      .CheckDynamicValidityOnly()
      .Build();
}

void InitializeFeatureState() {
  const auto& command_line = *base::CommandLine::ForCurrentProcess();
  base::HistogramSharedMemory::InitFromLaunchParameters(command_line);

  base::FieldTrialList field_trial_list;
  base::FieldTrialList::CreateTrialsInChildProcess(command_line);
  auto feature_list = std::make_unique<base::FeatureList>();
  base::FieldTrialList::ApplyFeatureOverridesInChildProcess(feature_list.get());
  base::FeatureList::SetInstance(std::move(feature_list));
}

}  // namespace

extern "C" {
// The entry point into the shortcut copier process. This is not
// a user API.
__attribute__((visibility("default"))) int ChromeWebAppShortcutCopierMain(
    int argc,
    char** argv);
}

// Installs the native app shim for a web app to its final location.
//
// When using ad-hoc signing for web app shims, the final app shim must be
// written to disk by this helper tool. This separate helper tool exists so that
// binary authorization tools, such as Santa, can transitively trust app shims
// that it creates without trusting all files written by Chrome. This allows app
// shims to be trusted by the binary authorization tool despite having only
// ad-hoc code signatures.
int ChromeWebAppShortcutCopierMain(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);

  // Override the path to the framework bundle so that it has a sensible value.
  // This tool lives within the Helpers subdirectory of the framework, so the
  // versioned path is two levels upwards.
  base::FilePath executable_path =
      base::PathService::CheckedGet(base::FILE_EXE);
  base::FilePath framework_path = executable_path.DirName().DirName();
  base::apple::SetOverrideFrameworkBundlePath(framework_path);

  // Matching what chrome::OuterAppBundle does, go up five steps from
  // C.app/Contents/Frameworks/C.framework/Versions/1.2.3.4 to C.app.
  base::FilePath outer_app_dir =
      framework_path.DirName().DirName().DirName().DirName().DirName();
  NSString* outer_app_dir_ns = base::SysUTF8ToNSString(outer_app_dir.value());
  NSBundle* base_bundle = [NSBundle bundleWithPath:outer_app_dir_ns];
  // In tests we might not be running from inside an app bundle, in that case
  // there is also no need to overide the bundle ID, as the default value should
  // already match that of the caller process.
  if (base_bundle && base_bundle.bundleIdentifier) {
    base::apple::SetBaseBundleID(base_bundle.bundleIdentifier.UTF8String);
  }

  auto requirement = CallerProcessRequirement();
  if (!requirement) {
    LOG(ERROR) << "Unable to construct requirement to validate caller";
    return 1;
  }
  base::MachPortRendezvousClientMac::SetServerProcessRequirement(
      std::move(*requirement));

  // Ensure that field trials and feature state matches that of Chrome.
  InitializeFeatureState();

  base::SingleThreadTaskExecutor main_task_executor{base::MessagePumpType::IO};

  mojo::core::InitFeatures();
  mojo::core::Init({.is_broker_process = true});
  mojo::core::ScopedIPCSupport ipc_support(
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN);

  mojo::PlatformChannelEndpoint endpoint =
      mojo::PlatformChannel::RecoverPassedEndpointFromCommandLine(
          *base::CommandLine::ForCurrentProcess());
  if (!endpoint.is_valid()) {
    LOG(ERROR) << "Unable to recover Mojo endpoint from command line.";
    return 1;
  }

  mojo::ScopedMessagePipeHandle pipe =
      mojo::IncomingInvitation::AcceptIsolated(std::move(endpoint));
  if (!pipe->is_valid()) {
    LOG(ERROR) << "Unable to accept Mojo invitation";
    return 1;
  }

  base::RunLoop run_loop;
  WebAppShortcutCopierImpl copier(
      mojo::PendingReceiver<web_app::mojom::WebAppShortcutCopier>(
          std::move(pipe)),
      run_loop.QuitWhenIdleClosure());
  run_loop.Run();

  return 0;
}
