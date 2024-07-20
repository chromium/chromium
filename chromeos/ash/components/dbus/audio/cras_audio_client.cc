// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/audio/cras_audio_client.h"

#include <stdint.h>

#include <utility>

#include "ash/constants/ash_switches.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/dbus/audio/fake_cras_audio_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"

namespace ash {

namespace {

CrasAudioClient* g_instance = nullptr;

// The CrasAudioClient implementation used in production.
class CrasAudioClientImpl : public CrasAudioClient {
 public:
  explicit CrasAudioClientImpl(dbus::Bus* bus) {
    cras_proxy_ = bus->GetObjectProxy(cras::kCrasServiceName,
                                      dbus::ObjectPath(cras::kCrasServicePath));

    // Monitor NameOwnerChanged signal.
    cras_proxy_->SetNameOwnerChangedCallback(
        base::BindRepeating(&CrasAudioClientImpl::NameOwnerChangedReceived,
                            weak_ptr_factory_.GetWeakPtr()));

    // Monitor the D-Bus signal for output mute change.
    cras_proxy_->ConnectToSignal(
        cras::kCrasControlInterface, cras::kOutputMuteChanged,
        base::BindRepeating(&CrasAudioClientImpl::OutputMuteChangedReceived,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&CrasAudioClientImpl::SignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));

    // Monitor the D-Bus signal for input mute change.
    cras_proxy_->ConnectToSignal(
        cras::kCrasControlInterface, cras::kInputMuteChanged,
        base::BindRepeating(&CrasAudioClientImpl::InputMuteChangedReceived,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&CrasAudioClientImpl::SignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));

    // Monitor the D-Bus signal for nodes change.
    cras_proxy_->ConnectToSignal(
        cras::kCrasControlInterface, cras::kNodesChanged,
        base::BindRepeating(&CrasAudioClientImpl::NodesChangedReceived,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&CrasAudioClientImpl::SignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));

    // Monitor the D-Bus signal for active output node change.
    cras_proxy_->ConnectToSignal(
        cras::kCrasControlInterface, cras::kActiveOutputNodeChanged,
        base::BindRepeating(
            &CrasAudioClientImpl::ActiveOutputNodeChangedReceived,
            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&CrasAudioClientImpl::SignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));

    // Monitor the D-Bus signal for active input node change.
    cras_proxy_->ConnectToSignal(
        cras::kCrasControlInterface, cras::kActiveInputNodeChanged,
        base::BindRepeating(
            &CrasAudioClientImpl::ActiveInputNodeChangedReceived,
            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&CrasAudioClientImpl::SignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));

    // Monitor the D-Bus signal for output node volume change.
    cras_proxy_->ConnectToSignal(
        cras::kCrasControlInterface, cras::kOutputNodeVolumeChanged,
        base::BindRepeating(
            &CrasAudioClientImpl::OutputNodeVolumeChangedReceived,
            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&CrasAudioClientImpl::SignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));

    // Monitor the D-Bus signal for input node gain change.
    cras_proxy_->ConnectToSignal(
        cras::kCrasControlInterface, cras::kInputNodeGainChanged,
        base::BindRepeating(&CrasAudioClientImpl::InputNodeGainChangedReceived,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&CrasAudioClientImpl::SignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));

    // Monitor the D-Bus signal for hotword.
    cras_proxy_->ConnectToSignal(
        cras::kCrasControlInterface, cras::kHotwordTriggered,
        base::BindRepeating(&CrasAudioClientImpl::HotwordTriggeredReceived,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&CrasAudioClientImpl::SignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));

    // Monitor the D-Bus signal for changes in number of active streams.
    cras_proxy_->ConnectToSignal(
        cras::kCrasControlInterface, cras::kNumberOfActiveStreamsChanged,
        base::BindRepeating(
            &CrasAudioClientImpl::NumberOfActiveStreamsChangedReceived,
            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&CrasAudioClientImpl::SignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));

    // Monitor the D-Bus signal for changes in number of input streams with
    // permission per client type.
    cras_proxy_->ConnectToSignal(
        cras::kCrasControlInterface,
        cras::kNumberOfInputStreamsWithPermissionChanged,
        base::BindRepeating(
            &CrasAudioClientImpl::NumberOfInputStreamsWithPermissionReceived,
            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&CrasAudioClientImpl::SignalConnected,
                       weak_ptr_factory_.GetWeakPtr())

    );

    // Monitor the D-Bus signal for changes in Bluetooth headset battery level.
    cras_proxy_->ConnectToSignal(
        cras::kCrasControlInterface, cras::kBluetoothBatteryChanged,
        base::BindRepeating(
            &CrasAudioClientImpl::BluetoothBatteryChangedReceived,
            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&CrasAudioClientImpl::SignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));

    // Monitor the D-Bus signal for CRAS to trigger an audio related survey.
    // The HaTS survey framework lives in Chrome and we use this signal as an
    // interface for CRAS to suggest when is proper to trigger the survey from
    // the system's perspective.
    cras_proxy_->ConnectToSignal(
        cras::kCrasControlInterface, cras::kSurveyTrigger,
        base::BindRepeating(&CrasAudioClientImpl::SurveyTriggerReceived,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&CrasAudioClientImpl::SignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));

    // Monitor the D-Bus signal for new speak-on-mute detection.
    cras_proxy_->ConnectToSignal(
        cras::kCrasControlInterface, cras::kSpeakOnMuteDetected,
        base::BindRepeating(&CrasAudioClientImpl::SpeakOnMuteDetectedReceived,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&CrasAudioClientImpl::SignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));

    cras_proxy_->ConnectToSignal(
        cras::kCrasControlInterface, "EwmaPowerReported",
        base::BindRepeating(&CrasAudioClientImpl::EwmaPowerReportedReceived,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&CrasAudioClientImpl::SignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));

    cras_proxy_->ConnectToSignal(
        cras::kCrasControlInterface,
        cras::kNumberOfNonChromeOutputStreamsChanged,
        base::BindRepeating(
            &CrasAudioClientImpl::NumberOfNonChromeOutputStreamsChangedReceived,
            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&CrasAudioClientImpl::SignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));

    // Monitor the D-Bus signal for is any stream ignore ui gains changed.
    cras_proxy_->ConnectToSignal(
        cras::kCrasControlInterface, cras::kNumStreamIgnoreUiGainsChanged,
        base::BindRepeating(
            &CrasAudioClientImpl::NumStreamIgnoreUiGainsReceived,
            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&CrasAudioClientImpl::SignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));

    // Monitor the D-Bus signal for number of ARC streams changed.
    cras_proxy_->ConnectToSignal(
        cras::kCrasControlInterface, cras::kNumberOfArcStreamsChanged,
        base::BindRepeating(
            &CrasAudioClientImpl::NumberOfArcStreamsChangedReceived,
            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&CrasAudioClientImpl::SignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));

    // Monitor the D-Bus signal for sidetone supported.
    cras_proxy_->ConnectToSignal(
        cras::kCrasControlInterface,
        "SidetoneSupportedChanged",  // TODO: Change to variable
        base::BindRepeating(
            &CrasAudioClientImpl::SidetoneSupportedChangedReceived,
            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&CrasAudioClientImpl::SignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  CrasAudioClientImpl(const CrasAudioClientImpl&) = delete;
  CrasAudioClientImpl& operator=(const CrasAudioClientImpl&) = delete;

  ~CrasAudioClientImpl() override = default;

  // CrasAudioClient overrides:
  void AddObserver(Observer* observer) override {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) override {
    observers_.RemoveObserver(observer);
  }

  bool HasObserver(const Observer* observer) const override {
    return observers_.HasObserver(observer);
  }

  void GetVolumeState(
      chromeos::DBusMethodCallback<VolumeState> callback) override {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kGetVolumeState);
    cras_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&CrasAudioClientImpl::OnGetVolumeState,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetDefaultOutputBufferSize(
      chromeos::DBusMethodCallback<int> callback) override {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kGetDefaultOutputBufferSize);
    cras_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&CrasAudioClientImpl::OnGetDefaultOutputBufferSize,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetSystemAecSupported(
      chromeos::DBusMethodCallback<bool> callback) override {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kGetSystemAecSupported);
    cras_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&CrasAudioClientImpl::OnGetSystemAecSupported,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetSystemAecGroupId(
      chromeos::DBusMethodCallback<int32_t> callback) override {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kGetSystemAecGroupId);

    cras_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&CrasAudioClientImpl::OnGetSystemAecGroupId,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetSystemNsSupported(
      chromeos::DBusMethodCallback<bool> callback) override {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kGetSystemNsSupported);
    cras_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&CrasAudioClientImpl::OnGetSystemNsSupported,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetSystemAgcSupported(
      chromeos::DBusMethodCallback<bool> callback) override {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kGetSystemAgcSupported);
    cras_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&CrasAudioClientImpl::OnGetSystemAgcSupported,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetNodes(chromeos::DBusMethodCallback<AudioNodeList> callback) override {
    dbus::MethodCall method_call(cras::kCrasControlInterface, cras::kGetNodes);
    cras_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&CrasAudioClientImpl::OnGetNodes,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetNumberOfActiveOutputStreams(
      chromeos::DBusMethodCallback<int> callback) override {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kGetNumberOfActiveOutputStreams);
    cras_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&CrasAudioClientImpl::OnGetNumberOfActiveOutputStreams,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetNumberOfInputStreamsWithPermission(
      chromeos::DBusMethodCallback<base::flat_map<std::string, uint32_t>>
          callback) override {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kGetNumberOfInputStreamsWithPermission);
    cras_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(
            &CrasAudioClientImpl::OnGetNumberOfInputStreamsWithPermission,
            weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetNumberOfNonChromeOutputStreams(
      chromeos::DBusMethodCallback<int32_t> callback) override {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kGetNumberOfNonChromeOutputStreams);
    cras_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(
            &CrasAudioClientImpl::OnGetNumberOfNonChromeOutputStreams,
            weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetSpeakOnMuteDetectionEnabled(
      chromeos::DBusMethodCallback<bool> callback) override {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kSpeakOnMuteDetectionEnabled);
    cras_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&CrasAudioClientImpl::OnGetSpeakOnMuteDetectionEnabled,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void SetOutputNodeVolume(uint64_t node_id, int32_t volume) override {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kSetOutputNodeVolume);
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint64(node_id);
    writer.AppendInt32(volume);
    cras_proxy_->CallMethod(&method_call,
                            dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                            base::DoNothing());
  }

  void SetOutputUserMute(bool mute_on) override {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kSetOutputUserMute);
    dbus::MessageWriter writer(&method_call);
    writer.AppendBool(mute_on);
    cras_proxy_->CallMethod(&method_call,
                            dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                            base::DoNothing());
  }

  void SetInputNodeGain(uint64_t node_id, int32_t input_gain) override {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kSetInputNodeGain);
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint64(node_id);
    writer.AppendInt32(input_gain);
    cras_proxy_->CallMethod(&method_call,
                            dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                            base::DoNothing());
  }

  void SetInputMute(bool mute_on) override {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kSetInputMute);
    dbus::MessageWriter writer(&method_call);
    writer.AppendBool(mute_on);
    cras_proxy_->CallMethod(&method_call,
                            dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                            base::DoNothing());
  }

  void SetNoiseCancellationEnabled(bool noise_cancellation_on) override {
    VLOG(1) << "cras_audio_client: Setting noise cancellation state: "
            << noise_cancellation_on;
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kSetNoiseCancellationEnabled);
    dbus::MessageWriter writer(&method_call);
    writer.AppendBool(noise_cancellation_on);
    cras_proxy_->CallMethod(&method_call,
                            dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                            base::DoNothing());
  }

  void GetNoiseCancellationSupported(
      chromeos::DBusMethodCallback<bool> callback) override {
    VLOG(1) << "cras_audio_client: Requesting noise cancellation support.";
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kIsNoiseCancellationSupported);
    cras_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&CrasAudioClientImpl::OnGetNoiseCancellationSupported,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void SetStyleTransferEnabled(bool style_transfer_on) override {
    VLOG(1) << "cras_audio_client: Setting style transfer state: "
            << style_transfer_on;
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kSetStyleTransferEnabled);

    dbus::MessageWriter writer(&method_call);
    writer.AppendBool(style_transfer_on);
    cras_proxy_->CallMethod(&method_call,
                            dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                            base::DoNothing());
  }

  void GetStyleTransferSupported(
      chromeos::DBusMethodCallback<bool> callback) override {
    VLOG(1) << "cras_audio_client: Requesting style transfer support.";
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kIsStyleTransferSupported);
    cras_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&CrasAudioClientImpl::OnGetStyleTransferSupported,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void SetHfpMicSrEnabled(bool hfp_mic_sr_on) override {
    VLOG(1) << "cras_audio_client: Setting hfp_mic_sr state: " << hfp_mic_sr_on;
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kSetHfpMicSrEnabled);
    dbus::MessageWriter writer(&method_call);
    writer.AppendBool(hfp_mic_sr_on);
    cras_proxy_->CallMethod(&method_call,
                            dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                            base::DoNothing());
  }

  void GetHfpMicSrSupported(
      chromeos::DBusMethodCallback<bool> callback) override {
    VLOG(1) << "cras_audio_client: Requesting hfp_mic_sr support.";
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kIsHfpMicSrSupported);
    cras_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&CrasAudioClientImpl::OnGetHfpMicSrSupported,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void SetActiveOutputNode(uint64_t node_id) override {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kSetActiveOutputNode);
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint64(node_id);
    cras_proxy_->CallMethod(&method_call,
                            dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                            base::DoNothing());
  }

  void SetActiveInputNode(uint64_t node_id) override {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kSetActiveInputNode);
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint64(node_id);
    cras_proxy_->CallMethod(&method_call,
                            dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                            base::DoNothing());
  }

  void SetHotwordModel(uint64_t node_id,
                       const std::string& hotword_model,
                       chromeos::VoidDBusMethodCallback callback) override {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kSetHotwordModel);
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint64(node_id);
    writer.AppendString(hotword_model);
    cras_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&CrasAudioClientImpl::OnSetHotwordModel,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void SetFixA2dpPacketSize(bool enabled) override {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kSetFixA2dpPacketSize);
    dbus::MessageWriter writer(&method_call);
    writer.AppendBool(enabled);
    cras_proxy_->CallMethod(&method_call,
                            dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                            base::DoNothing());
  }

  void SetFlossEnabled(bool enabled) override {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kSetFlossEnabled);
    dbus::MessageWriter writer(&method_call);
    writer.AppendBool(enabled);
    cras_proxy_->CallMethod(&method_call,
                            dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                            base::DoNothing());
  }

  void SetSpeakOnMuteDetection(bool enabled) override {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kSetSpeakOnMuteDetection);
    dbus::MessageWriter writer(&method_call);
    writer.AppendBool(enabled);
    cras_proxy_->CallMethod(&method_call,
                            dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                            base::DoNothing());
  }

  void SetEwmaPowerReportEnabled(bool enabled) override {
    dbus::MethodCall method_call(
        cras::kCrasControlInterface,
        "SetEwmaPowerReportEnabled");  // TODO change to variable;
    dbus::MessageWriter writer(&method_call);
    writer.AppendBool(enabled);
    cras_proxy_->CallMethod(&method_call,
                            dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                            base::DoNothing());
  }

  void SetSidetoneEnabled(bool enabled) override {
    dbus::MethodCall method_call(
        cras::kCrasControlInterface,
        "SetSidetoneEnabled");  // TODO change to variable;
    dbus::MessageWriter writer(&method_call);
    writer.AppendBool(enabled);
    cras_proxy_->CallMethod(&method_call,
                            dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                            base::DoNothing());
  }

  void GetSidetoneSupported(
      chromeos::DBusMethodCallback<bool> callback) override {
    dbus::MethodCall method_call(
        cras::kCrasControlInterface,
        "GetSidetoneSupported");  // TODO: Change to variable
    cras_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&CrasAudioClientImpl::OnGetSidetoneSupported,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void AddActiveInputNode(uint64_t node_id) override {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kAddActiveInputNode);
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint64(node_id);
    cras_proxy_->CallMethod(&method_call,
                            dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                            base::DoNothing());
  }

  void RemoveActiveInputNode(uint64_t node_id) override {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kRemoveActiveInputNode);
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint64(node_id);
    cras_proxy_->CallMethod(&method_call,
                            dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                            base::DoNothing());
  }

  void AddActiveOutputNode(uint64_t node_id) override {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kAddActiveOutputNode);
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint64(node_id);
    cras_proxy_->CallMethod(&method_call,
                            dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                            base::DoNothing());
  }

  void RemoveActiveOutputNode(uint64_t node_id) override {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kRemoveActiveOutputNode);
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint64(node_id);
    cras_proxy_->CallMethod(&method_call,
                            dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                            base::DoNothing());
  }

  void SwapLeftRight(uint64_t node_id, bool swap) override {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kSwapLeftRight);
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint64(node_id);
    writer.AppendBool(swap);
    cras_proxy_->CallMethod(&method_call,
                            dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                            base::DoNothing());
  }

  void SetDisplayRotation(uint64_t node_id,
                          cras::DisplayRotation rotation) override {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kSetDisplayRotation);
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint64(node_id);
    writer.AppendUint32(static_cast<uint32_t>(rotation));
    cras_proxy_->CallMethod(&method_call,
                            dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                            base::DoNothing());
  }

  void SetGlobalOutputChannelRemix(int32_t channels,
                                   const std::vector<double>& mixer) override {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kSetGlobalOutputChannelRemix);
    dbus::MessageWriter writer(&method_call);
    writer.AppendInt32(channels);
    writer.AppendArrayOfDoubles(mixer);
    cras_proxy_->CallMethod(&method_call,
                            dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                            base::DoNothing());
  }

  void SetPlayerPlaybackStatus(const std::string& playback_status) override {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kSetPlayerPlaybackStatus);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(playback_status);
    cras_proxy_->CallMethod(&method_call,
                            dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                            base::DoNothing());
  }

  void SetPlayerIdentity(const std::string& identity) override {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kSetPlayerIdentity);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(identity);
    cras_proxy_->CallMethod(&method_call,
                            dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                            base::DoNothing());
  }

  void SetPlayerPosition(const int64_t& position) override {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kSetPlayerPosition);
    dbus::MessageWriter writer(&method_call);
    writer.AppendInt64(position);
    cras_proxy_->CallMethod(&method_call,
                            dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                            base::DoNothing());
  }

  void SetPlayerDuration(const int64_t& duration) override {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kSetPlayerMetadata);
    dbus::MessageWriter writer(&method_call);
    dbus::MessageWriter array_writer(nullptr);
    dbus::MessageWriter dict_entry_writer(nullptr);

    writer.OpenArray("{sv}", &array_writer);
    array_writer.OpenDictEntry(&dict_entry_writer);
    dict_entry_writer.AppendString("length");
    dict_entry_writer.AppendVariantOfInt64(duration);
    array_writer.CloseContainer(&dict_entry_writer);
    writer.CloseContainer(&array_writer);

    cras_proxy_->CallMethod(&method_call,
                            dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                            base::DoNothing());
  }

  void SetPlayerMetadata(
      const std::map<std::string, std::string>& metadata) override {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kSetPlayerMetadata);
    dbus::MessageWriter writer(&method_call);
    dbus::MessageWriter array_writer(nullptr);
    dbus::MessageWriter dict_entry_writer(nullptr);

    writer.OpenArray("{sv}", &array_writer);

    for (auto& it : metadata) {
      array_writer.OpenDictEntry(&dict_entry_writer);
      dict_entry_writer.AppendString(it.first);
      dict_entry_writer.AppendVariantOfString(it.second);
      array_writer.CloseContainer(&dict_entry_writer);
    }

    writer.CloseContainer(&array_writer);

    cras_proxy_->CallMethod(&method_call,
                            dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                            base::DoNothing());
  }

  void ResendBluetoothBattery() override {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kResendBluetoothBattery);
    cras_proxy_->CallMethod(&method_call,
                            dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                            base::DoNothing());
  }

  void WaitForServiceToBeAvailable(
      chromeos::WaitForServiceToBeAvailableCallback callback) override {
    cras_proxy_->WaitForServiceToBeAvailable(std::move(callback));
  }

  void SetForceRespectUiGains(bool force_respect_ui_gains) override {
    VLOG(1) << "cras_audio_client: Setting force_respect_ui_gains state: "
            << force_respect_ui_gains;
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kSetForceRespectUiGains);
    dbus::MessageWriter writer(&method_call);
    writer.AppendBool(force_respect_ui_gains);
    cras_proxy_->CallMethod(&method_call,
                            dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                            base::DoNothing());
  }

  void GetNumStreamIgnoreUiGains(
      chromeos::DBusMethodCallback<int32_t> callback) override {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kGetNumStreamIgnoreUiGains);
    cras_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&CrasAudioClientImpl::OnGetNumStreamIgnoreUiGains,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetNumberOfArcStreams(
      chromeos::DBusMethodCallback<int32_t> callback) override {
    dbus::MethodCall method_call(cras::kCrasControlInterface,
                                 cras::kGetNumberOfArcStreams);
    cras_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&CrasAudioClientImpl::OnGetNumberOfArcStreams,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

 private:
  // Called when the cras signal is initially connected.
  void SignalConnected(const std::string& interface_name,
                       const std::string& signal_name,
                       bool success) {
    LOG_IF(ERROR, !success)
        << "Failed to connect to cras signal:" << signal_name;
  }

  void NameOwnerChangedReceived(const std::string& old_owner,
                                const std::string& new_owner) {
    for (auto& observer : observers_) {
      observer.AudioClientRestarted();
    }
  }

  // Called when a OutputMuteChanged signal is received.
  void OutputMuteChangedReceived(dbus::Signal* signal) {
    // Chrome should always call SetOutputUserMute api to set the output
    // mute state and monitor user_mute state from OutputMuteChanged signal.
    dbus::MessageReader reader(signal);
    bool system_mute, user_mute;
    if (!reader.PopBool(&system_mute) || !reader.PopBool(&user_mute)) {
      LOG(ERROR) << "Error reading signal from cras:" << signal->ToString();
    }
    for (auto& observer : observers_) {
      observer.OutputMuteChanged(user_mute);
    }
  }

  // Called when a InputMuteChanged signal is received.
  void InputMuteChangedReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    bool mute;
    if (!reader.PopBool(&mute)) {
      LOG(ERROR) << "Error reading signal from cras:" << signal->ToString();
    }
    for (auto& observer : observers_) {
      observer.InputMuteChanged(mute);
    }
  }

  void NodesChangedReceived(dbus::Signal* signal) {
    for (auto& observer : observers_) {
      observer.NodesChanged();
    }
  }

  void ActiveOutputNodeChangedReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    uint64_t node_id;
    if (!reader.PopUint64(&node_id)) {
      LOG(ERROR) << "Error reading signal from cras:" << signal->ToString();
    }
    for (auto& observer : observers_) {
      observer.ActiveOutputNodeChanged(node_id);
    }
  }

  void ActiveInputNodeChangedReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    uint64_t node_id;
    if (!reader.PopUint64(&node_id)) {
      LOG(ERROR) << "Error reading signal from cras:" << signal->ToString();
    }
    for (auto& observer : observers_) {
      observer.ActiveInputNodeChanged(node_id);
    }
  }

  void OutputNodeVolumeChangedReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    uint64_t node_id;
    int volume;

    if (!reader.PopUint64(&node_id)) {
      LOG(ERROR) << "Error reading signal from cras:" << signal->ToString();
    }
    if (!reader.PopInt32(&volume)) {
      LOG(ERROR) << "Error reading signal from cras:" << signal->ToString();
    }
    for (auto& observer : observers_) {
      observer.OutputNodeVolumeChanged(node_id, volume);
    }
  }

  void InputNodeGainChangedReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    uint64_t node_id;
    int gain;

    if (!reader.PopUint64(&node_id)) {
      LOG(ERROR) << "Error reading signal from cras:" << signal->ToString();
    }
    if (!reader.PopInt32(&gain)) {
      LOG(ERROR) << "Error reading signal from cras:" << signal->ToString();
    }
    for (auto& observer : observers_) {
      observer.InputNodeGainChanged(node_id, gain);
    }
  }

  void HotwordTriggeredReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    int64_t tv_sec, tv_nsec;

    if (!reader.PopInt64(&tv_sec)) {
      LOG(ERROR) << "Error reading signal from cras:" << signal->ToString();
      return;
    }

    if (!reader.PopInt64(&tv_nsec)) {
      LOG(ERROR) << "Error reading signal from cras:" << signal->ToString();
      return;
    }
    for (auto& observer : observers_) {
      observer.HotwordTriggered(tv_sec, tv_nsec);
    }
  }

  void NumberOfActiveStreamsChangedReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    // We skip reading the output because it is not used by the observers.
    for (auto& observer : observers_) {
      observer.NumberOfActiveStreamsChanged();
    }
  }

  void NumberOfInputStreamsWithPermissionReceived(dbus::Signal* signal) {
    dbus::MessageReader signal_reader(signal);
    dbus::MessageReader array_reader(nullptr);
    base::flat_map<std::string, uint32_t> res;
    while (signal_reader.HasMoreData()) {
      if (!signal_reader.PopArray(&array_reader)) {
        LOG(ERROR) << "Error reading signal from cras: " << signal->ToString();
        return;
      }
      std::string client_type;
      uint32_t num_input_streams;
      if (!GetNumerInputStreams(&array_reader, &client_type,
                                &num_input_streams)) {
        LOG(ERROR) << "Error reading number of input streams from cras: "
                   << signal->ToString();
        return;
      }
      res[client_type] = num_input_streams;
    }

    for (auto& observer : observers_) {
      observer.NumberOfInputStreamsWithPermissionChanged(res);
    }
  }

  void BluetoothBatteryChangedReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    std::string address;
    uint32_t level;

    if (!reader.PopString(&address)) {
      LOG(ERROR) << "Error reading signal from cras:" << signal->ToString();
      return;
    }

    if (!reader.PopUint32(&level)) {
      LOG(ERROR) << "Error reading signal from cras:" << signal->ToString();
      return;
    }
    for (auto& observer : observers_) {
      observer.BluetoothBatteryChanged(address, level);
    }
  }

  void OnGetVolumeState(chromeos::DBusMethodCallback<VolumeState> callback,
                        dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Error calling " << cras::kGetVolumeState;
      std::move(callback).Run(std::nullopt);
      return;
    }

    VolumeState volume_state;
    dbus::MessageReader reader(response);
    if (!reader.PopInt32(&volume_state.output_volume) ||
        !reader.PopBool(&volume_state.output_system_mute) ||
        !reader.PopInt32(&volume_state.input_gain) ||
        !reader.PopBool(&volume_state.input_mute) ||
        !reader.PopBool(&volume_state.output_user_mute)) {
      LOG(ERROR) << "Error reading response from cras: "
                 << response->ToString();
      std::move(callback).Run(std::nullopt);
      return;
    }

    std::move(callback).Run(std::move(volume_state));
  }

  bool PopSurveyKeyValue(dbus::MessageReader* array_reader,
                         std::string* key,
                         std::string* val) {
    dbus::MessageReader dict_entry_reader(nullptr);

    if (!array_reader->HasMoreData() ||
        !array_reader->PopDictEntry(&dict_entry_reader) ||
        !dict_entry_reader.PopString(key) ||
        !dict_entry_reader.PopVariantOfString(val)) {
      return false;
    }

    return true;
  }

  void SurveyTriggerReceived(dbus::Signal* signal) {
    dbus::MessageReader signal_reader(signal);
    dbus::MessageReader array_reader(nullptr);
    base::flat_map<std::string, std::string> res;

    while (signal_reader.HasMoreData()) {
      if (!signal_reader.PopArray(&array_reader)) {
        LOG(ERROR) << "Error reading signal from cras: " << signal->ToString();
        return;
      }

      // Chrome HaTS survey system supports logging a map of survey specific
      // data for better data interpretation.
      // For general audio satisfaction survey as an example, we shall get the
      // snapshot of stream type, client type and also active node type pair
      // when the user closes a stream living longer than a specified perioid
      // of time. We can use the data to identify the user scenario we'd like
      // to improve, such as voice communication on Chrome through Bluetooth.
      std::string key;
      std::string val;
      while (array_reader.HasMoreData()) {
        if (!PopSurveyKeyValue(&array_reader, &key, &val)) {
          LOG(ERROR) << "Error reading key value pairs of the data from cras: "
                     << signal->ToString();
          return;
        }

        res[key] = val;
      }
    }

    for (auto& observer : observers_) {
      observer.SurveyTriggered(res);
    }
  }

  void SpeakOnMuteDetectedReceived(dbus::Signal* signal) {
    for (auto& observer : observers_) {
      observer.SpeakOnMuteDetected();
    }
  }

  void EwmaPowerReportedReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    double power;
    if (!reader.PopDouble(&power)) {
      LOG(ERROR) << "Error reading signal from cras: " << signal->ToString();
    }
    for (auto& observer : observers_) {
      observer.EwmaPowerReported(power);
    }
  }

  void NumberOfNonChromeOutputStreamsChangedReceived(dbus::Signal* signal) {
    for (auto& observer : observers_) {
      observer.NumberOfNonChromeOutputStreamsChanged();
    }
  }

  void NumStreamIgnoreUiGainsReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    int32_t num;
    if (!reader.PopInt32(&num)) {
      LOG(ERROR) << "Error reading signal from cras:" << signal->ToString();
    }
    for (auto& observer : observers_) {
      observer.NumStreamIgnoreUiGains(num);
    }
  }

  void NumberOfArcStreamsChangedReceived(dbus::Signal* signal) {
    for (auto& observer : observers_) {
      observer.NumberOfArcStreamsChanged();
    }
  }

  void SidetoneSupportedChangedReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    bool supported;
    if (!reader.PopBool(&supported)) {
      LOG(ERROR) << "Error reading signal from cras: " << signal->ToString();
    }
    for (auto& observer : observers_) {
      observer.SidetoneSupportedChanged(supported);
    }
  }

  void OnGetDefaultOutputBufferSize(chromeos::DBusMethodCallback<int> callback,
                                    dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Error calling " << cras::kGetDefaultOutputBufferSize;
      std::move(callback).Run(std::nullopt);
      return;
    }
    int32_t buffer_size = 0;
    dbus::MessageReader reader(response);
    if (!reader.PopInt32(&buffer_size)) {
      LOG(ERROR) << "Error reading response from cras: "
                 << response->ToString();
      std::move(callback).Run(std::nullopt);
      return;
    }

    std::move(callback).Run(buffer_size);
  }

  void OnGetSystemAecSupported(chromeos::DBusMethodCallback<bool> callback,
                               dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Error calling " << cras::kGetSystemAecSupported;
      std::move(callback).Run(std::nullopt);
      return;
    }
    bool system_aec_supported = 0;
    dbus::MessageReader reader(response);
    if (!reader.PopBool(&system_aec_supported)) {
      LOG(ERROR) << "Error reading response from cras: "
                 << response->ToString();
      std::move(callback).Run(std::nullopt);
      return;
    }

    std::move(callback).Run(system_aec_supported);
  }

  void OnGetSystemAecGroupId(chromeos::DBusMethodCallback<int32_t> callback,
                             dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Error calling " << cras::kGetSystemAecGroupId;
      std::move(callback).Run(std::nullopt);
      return;
    }
    int32_t system_aec_group_id = 0;
    dbus::MessageReader reader(response);
    if (!reader.PopInt32(&system_aec_group_id)) {
      LOG(ERROR) << "Error reading response from cras: "
                 << response->ToString();
      std::move(callback).Run(std::nullopt);
      return;
    }

    std::move(callback).Run(system_aec_group_id);
  }

  void OnGetSystemNsSupported(chromeos::DBusMethodCallback<bool> callback,
                              dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Error calling " << cras::kGetSystemNsSupported;
      std::move(callback).Run(std::nullopt);
      return;
    }
    bool system_ns_supported = 0;
    dbus::MessageReader reader(response);
    if (!reader.PopBool(&system_ns_supported)) {
      LOG(ERROR) << "Error reading response from cras: "
                 << response->ToString();
      std::move(callback).Run(std::nullopt);
      return;
    }

    std::move(callback).Run(system_ns_supported);
  }

  void OnGetSystemAgcSupported(chromeos::DBusMethodCallback<bool> callback,
                               dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Error calling " << cras::kGetSystemAgcSupported;
      std::move(callback).Run(std::nullopt);
      return;
    }
    bool system_agc_supported = 0;
    dbus::MessageReader reader(response);
    if (!reader.PopBool(&system_agc_supported)) {
      LOG(ERROR) << "Error reading response from cras: "
                 << response->ToString();
      std::move(callback).Run(std::nullopt);
      return;
    }

    std::move(callback).Run(system_agc_supported);
  }

  void OnGetNodes(chromeos::DBusMethodCallback<AudioNodeList> callback,
                  dbus::Response* response) {
    if (!response) {
      std::move(callback).Run(std::nullopt);
      return;
    }

    AudioNodeList node_list;
    dbus::MessageReader response_reader(response);
    dbus::MessageReader array_reader(response);
    while (response_reader.HasMoreData()) {
      if (!response_reader.PopArray(&array_reader)) {
        LOG(ERROR) << "Error reading response from cras: "
                   << response->ToString();
        std::move(callback).Run(std::nullopt);
        return;
      }

      AudioNode node;
      if (!GetAudioNode(response, &array_reader, &node)) {
        LOG(WARNING) << "Error reading audio node data from cras: "
                     << response->ToString();
        std::move(callback).Run(std::nullopt);
        return;
      }

      // Filter out the "UNKNOWN" type of audio devices.
      if (node.type != "UNKNOWN") {
        node_list.push_back(std::move(node));
      }
    }

    std::move(callback).Run(std::move(node_list));
  }

  void OnGetSidetoneSupported(chromeos::DBusMethodCallback<bool> callback,
                              dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Error calling GetSidetoneSupported";
      std::move(callback).Run(std::nullopt);
      return;
    }
    bool available = false;
    dbus::MessageReader reader(response);
    if (!reader.PopBool(&available)) {
      LOG(ERROR) << "Error reading response from cras: "
                 << response->ToString();
      std::move(callback).Run(std::nullopt);
      return;
    }
    std::move(callback).Run(available);
  }

  void OnGetNumberOfNonChromeOutputStreams(
      chromeos::DBusMethodCallback<int32_t> callback,
      dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Error calling "
                 << cras::kGetNumberOfNonChromeOutputStreams;
      std::move(callback).Run(std::nullopt);
      return;
    }
    int32_t num_active_streams = 0;
    dbus::MessageReader reader(response);
    if (!reader.PopInt32(&num_active_streams)) {
      LOG(ERROR) << "Error reading response from cras: "
                 << response->ToString();
      std::move(callback).Run(std::nullopt);
      return;
    }
    std::move(callback).Run(num_active_streams);
  }

  void OnGetNumberOfActiveOutputStreams(
      chromeos::DBusMethodCallback<int> callback,
      dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Error calling " << cras::kGetNumberOfActiveOutputStreams;
      std::move(callback).Run(std::nullopt);
      return;
    }
    int32_t num_active_streams = 0;
    dbus::MessageReader reader(response);
    if (!reader.PopInt32(&num_active_streams)) {
      LOG(ERROR) << "Error reading response from cras: "
                 << response->ToString();
      std::move(callback).Run(std::nullopt);
      return;
    }

    std::move(callback).Run(num_active_streams);
  }

  bool GetNumerInputStreams(dbus::MessageReader* array_reader,
                            std::string* client_type,
                            uint32_t* num_input_streams) {
    while (array_reader->HasMoreData()) {
      dbus::MessageReader dict_entry_reader(nullptr);
      dbus::MessageReader value_reader(nullptr);
      std::string key;
      if (!array_reader->PopDictEntry(&dict_entry_reader) ||
          !dict_entry_reader.PopString(&key) ||
          !dict_entry_reader.PopVariant(&value_reader)) {
        return false;
      }

      if (key == cras::kClientType) {
        if (!value_reader.PopString(client_type)) {
          return false;
        }
      } else if (key == cras::kNumStreamsWithPermission) {
        if (!value_reader.PopUint32(num_input_streams)) {
          return false;
        }
      }
    }
    return true;
  }

  void OnGetNumberOfInputStreamsWithPermission(
      chromeos::DBusMethodCallback<base::flat_map<std::string, uint32_t>>
          callback,
      dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Error calling "
                 << cras::kGetNumberOfInputStreamsWithPermission;
      std::move(callback).Run(std::nullopt);
      return;
    }
    dbus::MessageReader response_reader(response);
    dbus::MessageReader array_reader(nullptr);
    base::flat_map<std::string, uint32_t> res;
    while (response_reader.HasMoreData()) {
      if (!response_reader.PopArray(&array_reader)) {
        LOG(ERROR) << "Error reading response from cras: "
                   << response->ToString();
        std::move(callback).Run(std::nullopt);
        return;
      }
      std::string client_type;
      uint32_t num_input_streams;
      if (!GetNumerInputStreams(&array_reader, &client_type,
                                &num_input_streams)) {
        LOG(ERROR) << "Error reading number of input streams from cras: "
                   << response->ToString();
        std::move(callback).Run(std::nullopt);
        return;
      }
      res[client_type] = num_input_streams;
    }

    std::move(callback).Run(std::move(res));
  }

  void OnSetHotwordModel(chromeos::VoidDBusMethodCallback callback,
                         dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Failed to call SetHotwordModel.";
      std::move(callback).Run(false);
      return;
    }

    dbus::MessageReader reader(response);
    int32_t result;
    if (!reader.PopInt32(&result)) {
      LOG(ERROR) << "Failed to parse results from SetHotwordModel.";
      std::move(callback).Run(false);
      return;
    }

    if (result != 0) {
      LOG(ERROR) << "Errors in SetHotwordModel.";
      std::move(callback).Run(false);
      return;
    }

    std::move(callback).Run(true);
  }

  void OnGetNoiseCancellationSupported(
      chromeos::DBusMethodCallback<bool> callback,
      dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Error calling "
                 << "GetNoiseCancellationSupported";
      std::move(callback).Run(std::nullopt);
      return;
    }
    bool is_noise_cancellation_supported = 0;
    dbus::MessageReader reader(response);
    if (!reader.PopBool(&is_noise_cancellation_supported)) {
      LOG(ERROR) << "Error reading response from cras: "
                 << response->ToString();
      std::move(callback).Run(std::nullopt);
      return;
    }
    std::move(callback).Run(is_noise_cancellation_supported);
    VLOG(1) << "cras_audio_client: Retrieved noise cancellation support: "
            << is_noise_cancellation_supported;
  }

  void OnGetStyleTransferSupported(chromeos::DBusMethodCallback<bool> callback,
                                   dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Error calling GetStyleTransferSupported";
      std::move(callback).Run(std::nullopt);
      return;
    }
    bool is_style_transfer_supported = 0;
    dbus::MessageReader reader(response);
    if (!reader.PopBool(&is_style_transfer_supported)) {
      LOG(ERROR) << "Error reading response from cras: "
                 << response->ToString();
      std::move(callback).Run(std::nullopt);
      return;
    }
    std::move(callback).Run(is_style_transfer_supported);
    VLOG(1) << "cras_audio_client: Retrieved style transfer support: "
            << is_style_transfer_supported;
  }

  void OnGetHfpMicSrSupported(chromeos::DBusMethodCallback<bool> callback,
                              dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Error calling "
                 << "IsHfpMicSrSupported";
      std::move(callback).Run(std::nullopt);
      return;
    }
    bool is_hfp_mic_sr_supported = 0;
    dbus::MessageReader reader(response);
    if (!reader.PopBool(&is_hfp_mic_sr_supported)) {
      LOG(ERROR) << "Error reading response from cras: "
                 << response->ToString();
      std::move(callback).Run(std::nullopt);
      return;
    }
    std::move(callback).Run(is_hfp_mic_sr_supported);
    VLOG(1) << "cras_audio_client: Retrieved hfp_mic_sr support: "
            << is_hfp_mic_sr_supported;
  }

  bool GetAudioNode(dbus::Response* response,
                    dbus::MessageReader* array_reader,
                    AudioNode* node) {
    while (array_reader->HasMoreData()) {
      dbus::MessageReader dict_entry_reader(response);
      dbus::MessageReader value_reader(response);
      std::string key;
      if (!array_reader->PopDictEntry(&dict_entry_reader) ||
          !dict_entry_reader.PopString(&key) ||
          !dict_entry_reader.PopVariant(&value_reader)) {
        return false;
      }

      if (key == cras::kIsInputProperty) {
        if (!value_reader.PopBool(&node->is_input)) {
          return false;
        }
      } else if (key == cras::kIdProperty) {
        if (!value_reader.PopUint64(&node->id)) {
          return false;
        }
      } else if (key == cras::kDeviceNameProperty) {
        if (!value_reader.PopString(&node->device_name)) {
          return false;
        }
      } else if (key == cras::kTypeProperty) {
        if (!value_reader.PopString(&node->type)) {
          return false;
        }
      } else if (key == cras::kNameProperty) {
        if (!value_reader.PopString(&node->name)) {
          return false;
        }
      } else if (key == cras::kActiveProperty) {
        if (!value_reader.PopBool(&node->active)) {
          return false;
        }
      } else if (key == cras::kPluggedTimeProperty) {
        if (!value_reader.PopUint64(&node->plugged_time)) {
          return false;
        }
      } else if (key == cras::kStableDeviceIdProperty) {
        if (!value_reader.PopUint64(&node->stable_device_id_v1)) {
          return false;
        }
      } else if (key == cras::kStableDeviceIdNewProperty) {
        if (!value_reader.PopUint64(&node->stable_device_id_v2)) {
          return false;
        }
        node->has_v2_stable_device_id = true;
      } else if (key == cras::kMaxSupportedChannelsProperty) {
        if (!value_reader.PopUint32(&node->max_supported_channels)) {
          return false;
        }
      } else if (key == cras::kAudioEffectProperty) {
        if (!value_reader.PopUint32(&node->audio_effect)) {
          return false;
        }
      } else if (key == cras::kNumberOfVolumeStepsProperty) {
        if (!value_reader.PopInt32(&node->number_of_volume_steps)) {
          return false;
        }
      }
    }

    return true;
  }

  void OnGetSpeakOnMuteDetectionEnabled(
      chromeos::DBusMethodCallback<bool> callback,
      dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Error calling "
                 << "GetSpeakOnMuteDetectionEnabled";
      std::move(callback).Run(std::nullopt);
      return;
    }
    bool speak_on_mute_detection_enabled = false;
    dbus::MessageReader reader(response);
    if (!reader.PopBool(&speak_on_mute_detection_enabled)) {
      LOG(ERROR) << "Error reading response from cras: "
                 << response->ToString();
      std::move(callback).Run(std::nullopt);
      return;
    }
    std::move(callback).Run(speak_on_mute_detection_enabled);
  }

  void OnGetNumStreamIgnoreUiGains(
      chromeos::DBusMethodCallback<int32_t> callback,
      dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Error calling " << cras::kGetNumStreamIgnoreUiGains;
      std::move(callback).Run(std::nullopt);
      return;
    }
    int32_t num_stream_ignore_ui_gains = 0;
    dbus::MessageReader reader(response);
    if (!reader.PopInt32(&num_stream_ignore_ui_gains)) {
      LOG(ERROR) << "Error reading response from cras: "
                 << response->ToString();
      std::move(callback).Run(std::nullopt);
      return;
    }
    std::move(callback).Run(num_stream_ignore_ui_gains);
  }

  void OnGetNumberOfArcStreams(chromeos::DBusMethodCallback<int32_t> callback,
                               dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Error calling " << cras::kGetNumberOfArcStreams;
      std::move(callback).Run(std::nullopt);
      return;
    }
    int32_t num_arc_streams = 0;
    dbus::MessageReader reader(response);
    if (!reader.PopInt32(&num_arc_streams)) {
      LOG(ERROR) << "Error reading response from cras: "
                 << response->ToString();
      std::move(callback).Run(std::nullopt);
      return;
    }
    std::move(callback).Run(num_arc_streams);
  }

  raw_ptr<dbus::ObjectProxy> cras_proxy_ = nullptr;
  base::ObserverList<Observer>::Unchecked observers_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<CrasAudioClientImpl> weak_ptr_factory_{this};
};

}  // namespace

CrasAudioClient::Observer::~Observer() = default;

void CrasAudioClient::Observer::AudioClientRestarted() {}

void CrasAudioClient::Observer::OutputMuteChanged(bool mute_on) {}

void CrasAudioClient::Observer::InputMuteChanged(bool mute_on) {}

void CrasAudioClient::Observer::NodesChanged() {}

void CrasAudioClient::Observer::ActiveOutputNodeChanged(uint64_t node_id) {}

void CrasAudioClient::Observer::ActiveInputNodeChanged(uint64_t node_id) {}

void CrasAudioClient::Observer::OutputNodeVolumeChanged(uint64_t node_id,
                                                        int volume) {}

void CrasAudioClient::Observer::InputNodeGainChanged(uint64_t node_id,
                                                     int gain) {}

void CrasAudioClient::Observer::HotwordTriggered(uint64_t tv_sec,
                                                 uint64_t tv_nsec) {}

void CrasAudioClient::Observer::NumberOfActiveStreamsChanged() {}

void CrasAudioClient::Observer::BluetoothBatteryChanged(
    const std::string& address,
    uint32_t level) {}

void CrasAudioClient::Observer::NumberOfInputStreamsWithPermissionChanged(
    const base::flat_map<std::string, uint32_t>& num_input_streams) {}

void CrasAudioClient::Observer::SurveyTriggered(
    const base::flat_map<std::string, std::string>& survey_specific_data) {}

void CrasAudioClient::Observer::SpeakOnMuteDetected() {}

void CrasAudioClient::Observer::EwmaPowerReported(double power) {}

void CrasAudioClient::Observer::NumberOfNonChromeOutputStreamsChanged() {}

void CrasAudioClient::Observer::NumStreamIgnoreUiGains(int32_t num) {}

void CrasAudioClient::Observer::NumberOfArcStreamsChanged() {}

void CrasAudioClient::Observer::SidetoneSupportedChanged(bool supported) {}

CrasAudioClient::CrasAudioClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

CrasAudioClient::~CrasAudioClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void CrasAudioClient::Initialize(dbus::Bus* bus) {
  DCHECK(bus);
  if (ash::switches::UseFakeCrasAudioClientForDBus()) {
    LOG(WARNING) << "Using FakeCrasAudioClient due to switch: "
                 << ash::switches::kUseFakeCrasAudioClientForDBus;
    InitializeFake();
  } else {
    new CrasAudioClientImpl(bus);
  }
}

// static
void CrasAudioClient::InitializeFake() {
  new FakeCrasAudioClient();
}

// static
void CrasAudioClient::Shutdown() {
  delete g_instance;
}

// static
CrasAudioClient* CrasAudioClient::Get() {
  return g_instance;
}

}  // namespace ash
