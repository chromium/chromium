// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

if ((typeof mojo === 'undefined') || !mojo.bindingsLibraryInitialized) {
  loadScript('mojo_bindings');
}
mojo.config.autoLoadMojomDeps = false;

loadScript('chrome/common/media_router/mojo/media_controller.mojom');
loadScript('chrome/common/media_router/mojo/media_router.mojom');
loadScript('chrome/common/media_router/mojo/media_status.mojom');
loadScript('components/mirroring/mojom/cast_message_channel.mojom');
loadScript('components/mirroring/mojom/mirroring_service_host.mojom');
loadScript('components/mirroring/mojom/session_observer.mojom');
loadScript('components/mirroring/mojom/session_parameters.mojom');
loadScript('extensions/common/mojo/keep_alive.mojom');
loadScript('media/mojo/interfaces/mirror_service_remoting.mojom');
loadScript('media/mojo/interfaces/remoting_common.mojom');
loadScript('mojo/public/mojom/base/time.mojom');
loadScript('mojo/public/mojom/base/unguessable_token.mojom');
loadScript('net/interfaces/ip_address.mojom');
loadScript('net/interfaces/ip_endpoint.mojom');
loadScript('url/mojom/origin.mojom');
loadScript('url/mojom/url.mojom');

// The following adapter classes preserve backward compatibility for the media
// router component extension.
// TODO(crbug.com/787128): Remove these adapters.

function assignFields(object, fields) {
  for(var field in fields) {
    if (object.hasOwnProperty(field))
      object[field] = fields[field];
  }
}

/**
 * Adapter for mediaRouter.mojom.DialMediaSink.
 * @constructor
 */
function DialMediaSinkAdapter(fields) {
  this.ip_address = null;
  this.model_name = null;
  this.app_url = null;

  assignFields(this, fields);
}

DialMediaSinkAdapter.fromNewVersion = function(other) {
  return new DialMediaSinkAdapter({
    'ip_address': IPAddressAdapter.fromNewVersion(other.ipAddress),
    'model_name': other.modelName,
    'app_url': other.appUrl,
  });
};

DialMediaSinkAdapter.prototype.toNewVersion = function() {
  return new mediaRouter.mojom.DialMediaSink({
    'ipAddress' : this.ip_address.toNewVersion(),
    'modelName' : this.model_name,
    'appUrl' : this.app_url,
  });
};

/**
 * Adapter for mediaRouter.mojom.CastMediaSink.
 * @constructor
 */
function CastMediaSinkAdapter(fields) {
  this.ip_endpoint = null;
  this.model_name = null;
  this.capabilities = 0;
  this.cast_channel_id = 0;

  assignFields(this, fields);
}

CastMediaSinkAdapter.fromNewVersion = function(other) {
  return new CastMediaSinkAdapter({
    'ip_endpoint': IPEndpointAdapter.fromNewVersion(other.ipEndpoint),
    'model_name': other.modelName,
    'capabilities': other.capabilities,
    'cast_channel_id': other.castChannelId,
  });
};

CastMediaSinkAdapter.prototype.toNewVersion = function() {
  return new mediaRouter.mojom.CastMediaSink({
    'ipEndpoint': this.ip_endpoint.toNewVersion(),
    'modelName': this.model_name,
    'capabilities': this.capabilities,
    'castChannelId': this.cast_channel_id,
  });
};

/**
 * Adapter for mediaRouter.mojom.HangoutsMediaStatusExtraData.
 * @constructor
 */
function HangoutsMediaStatusExtraDataAdapter(fields) {
  this.local_present = false;

  assignFields(this, fields);
}

HangoutsMediaStatusExtraDataAdapter.prototype.toNewVersion = function() {
  return new mediaRouter.mojom.HangoutsMediaStatusExtraData({
    'localPresent': this.local_present,
  });
};

/**
 * Adapter for net.interfaces.IPAddress.
 * @constructor
 */
function IPAddressAdapter(fields) {
  this.address_bytes = null;

  assignFields(this, fields);
}

IPAddressAdapter.fromNewVersion = function(other) {
  return new IPAddressAdapter({
    'address_bytes': other.addressBytes,
  });
};

IPAddressAdapter.prototype.toNewVersion = function() {
  return new net.interfaces.IPAddress({
    'addressBytes': this.address_bytes,
  });
};

/**
 * Adapter for net.interfaces.IPEndpoint.
 * @constructor
 */
function IPEndpointAdapter(fields) {
  this.address = null;
  this.port = 0;

  assignFields(this, fields);
}

IPEndpointAdapter.fromNewVersion = function(other) {
  return new IPEndpointAdapter({
    'address': IPAddressAdapter.fromNewVersion(other.address),
    'port': other.port,
  });
};

IPEndpointAdapter.prototype.toNewVersion = function() {
  return new net.interfaces.IPEndpoint({
    'address': this.address.toNewVersion(),
    'port': this.port,
  });
};

/**
 * Adapter for mediaRouter.mojom.MediaStatus.
 * @constructor
 */
function MediaStatusAdapter(fields) {
  this.title = null;
  this.can_play_pause = false;
  this.can_mute = false;
  this.can_set_volume = false;
  this.can_seek = false;
  this.is_muted = false;
  this.play_state = 0;
  this.volume = 0;
  this.duration = null;
  this.current_time = null;
  this.hangouts_extra_data = null;

  assignFields(this, fields);
}

MediaStatusAdapter.PlayState = mediaRouter.mojom.MediaStatus.PlayState;

MediaStatusAdapter.prototype.toNewVersion = function() {
  return new mediaRouter.mojom.MediaStatus({
    'title': this.title,
    'canPlayPause': this.can_play_pause,
    'canMute': this.can_mute,
    'canSetVolume': this.can_set_volume,
    'canSeek': this.can_seek,
    'isMuted': this.is_muted,
    'playState': this.play_state,
    'volume': this.volume,
    'duration': this.duration,
    'currentTime': this.current_time,
    'hangoutsExtraData':
        this.hangouts_extra_data && this.hangouts_extra_data.toNewVersion(),
  });
};

/**
 * Adapter for media.mojom.RemotingSinkMetadata.
 * @constructor
 */
function RemotingSinkMetadataAdapter(fields) {
  this.features = null;
  this.audio_capabilities = null;
  this.video_capabilities = null;
  this.friendly_name = null;

  assignFields(this, fields);
}

RemotingSinkMetadataAdapter.fromNewVersion = function(other) {
  return new RemotingSinkMetadataAdapter({
    'features': other.features,
    'audio_capabilities': other.audioCapabilities,
    'video_capabilities': other.videoCapabilities,
    'friendly_name': other.friendlyName,
  });
};

RemotingSinkMetadataAdapter.prototype.toNewVersion = function() {
  return new media.mojom.RemotingSinkMetadata({
    'features': this.features,
    'audioCapabilities': this.audio_capabilities,
    'videoCapabilities': this.video_capabilities,
    'friendlyName': this.friendly_name,
  });
};

/**
 * Adapter for mediaRouter.mojom.MediaSink.
 * @constructor
 */
function MediaSinkAdapter(fields) {
  this.sink_id = null;
  this.name = null;
  this.description = null;
  this.domain = null;
  this.icon_type = 0;
  this.extra_data = null;

  assignFields(this, fields);
}

MediaSinkAdapter.fromNewVersion = function(other) {
  return new MediaSinkAdapter({
    'sink_id': other.sinkId,
    'name': other.name,
    'description': other.description,
    'domain': other.domain,
    'icon_type': other.iconType,
    'extra_data': other.extraData &&
        MediaSinkExtraDataAdapter.fromNewVersion(other.extraData),
  });
};

MediaSinkAdapter.prototype.toNewVersion = function() {
  return new mediaRouter.mojom.MediaSink({
    'sinkId': this.sink_id,
    'name': this.name,
    'description': this.description,
    'domain': this.domain,
    'iconType': this.icon_type,
    'extraData': this.extra_data && this.extra_data.toNewVersion(),
  });
};

/**
 * Adapter for mediaRouter.mojom.MediaSinkExtraData.
 * @constructor
 */
function MediaSinkExtraDataAdapter(value) {
  this.$data = null;
  this.$tag = undefined;

  if (value == undefined) {
    return;
  }

  var keys = Object.keys(value);
  if (keys.length == 0) {
    return;
  }

  if (keys.length > 1) {
    throw new TypeError('You may set only one member on a union.');
  }

  var fields = [
    'dial_media_sink',
    'cast_media_sink',
  ];

  if (fields.indexOf(keys[0]) < 0) {
    throw new ReferenceError(keys[0] +
        ' is not a MediaSinkExtraDataAdapter member.');
  }

  this[keys[0]] = value[keys[0]];
}

MediaSinkExtraDataAdapter.Tags = {
  dial_media_sink: 0,
  cast_media_sink: 1,
};

Object.defineProperty(MediaSinkExtraDataAdapter.prototype, 'dial_media_sink', {
  get: function() {
    if (this.$tag != MediaSinkExtraDataAdapter.Tags.dial_media_sink) {
      throw new ReferenceError(
          'MediaSinkExtraDataAdapter.dial_media_sink is not currently set.');
    }
    return this.$data;
  },

  set: function(value) {
    this.$tag = MediaSinkExtraDataAdapter.Tags.dial_media_sink;
    this.$data = value;
  }
});

Object.defineProperty(MediaSinkExtraDataAdapter.prototype, 'cast_media_sink', {
  get: function() {
    if (this.$tag != MediaSinkExtraDataAdapter.Tags.cast_media_sink) {
      throw new ReferenceError(
          'MediaSinkExtraDataAdapter.cast_media_sink is not currently set.');
    }
    return this.$data;
  },

  set: function(value) {
    this.$tag = MediaSinkExtraDataAdapter.Tags.cast_media_sink;
    this.$data = value;
  }
});

MediaSinkExtraDataAdapter.fromNewVersion = function(other) {
  if (other.$tag == mediaRouter.mojom.MediaSinkExtraData.Tags.dialMediaSink) {
    return new MediaSinkExtraDataAdapter({
      'dial_media_sink':
          DialMediaSinkAdapter.fromNewVersion(other.dialMediaSink),
    });
  } else {
    return new MediaSinkExtraDataAdapter({
      'cast_media_sink':
          CastMediaSinkAdapter.fromNewVersion(other.castMediaSink),
    });
  }
};

MediaSinkExtraDataAdapter.prototype.toNewVersion = function() {
  if (this.$tag == MediaSinkExtraDataAdapter.Tags.dial_media_sink) {
    return new mediaRouter.mojom.MediaSinkExtraData({
      'dialMediaSink': this.dial_media_sink.toNewVersion(),
    });
  } else {
    return new mediaRouter.mojom.MediaSinkExtraData({
      'castMediaSink': this.cast_media_sink.toNewVersion(),
    });
  }
};

/**
 * Adapter for media.mojom.MirrorServiceRemoterPtr.
 * @constructor
 */
function MirrorServiceRemoterPtrAdapter(handleOrPtrInfo) {
  this.ptr = new mojo.InterfacePtrController(MirrorServiceRemoterAdapter,
                                             handleOrPtrInfo);
}

MirrorServiceRemoterPtrAdapter.prototype =
    Object.create(media.mojom.MirrorServiceRemoterPtr.prototype);
MirrorServiceRemoterPtrAdapter.prototype.constructor =
    MirrorServiceRemoterPtrAdapter;

MirrorServiceRemoterPtrAdapter.prototype.startDataStreams = function() {
  return MirrorServiceRemoterProxy.prototype.startDataStreams
      .apply(this.ptr.getProxy(), arguments).then(function(response) {
    return Promise.resolve({
      'audio_stream_id': response.audioStreamId,
      'video_stream_id': response.videoStreamId,
    });
  });
};

/**
 * Adapter for media.mojom.MirrorServiceRemoter.stubclass.
 * @constructor
 */
function MirrorServiceRemoterStubAdapter(delegate) {
  this.delegate_ = delegate;
}

MirrorServiceRemoterStubAdapter.prototype = Object.create(
    media.mojom.MirrorServiceRemoter.stubClass.prototype);
MirrorServiceRemoterStubAdapter.prototype.constructor =
    MirrorServiceRemoterStubAdapter;

MirrorServiceRemoterStubAdapter.prototype.startDataStreams =
    function(hasAudio, hasVideo) {
  return this.delegate_ && this.delegate_.startDataStreams &&
      this.delegate_.startDataStreams(hasAudio, hasVideo).then(
          function(response) {
            return {
              'audioStreamId': response.audio_stream_id,
              'videoStreamId': response.video_stream_id,
            };
          });
};

/**
 * Adapter for media.mojom.MirrorServiceRemoter.
 */
var MirrorServiceRemoterAdapter = {
    name: 'media.mojom.MirrorServiceRemoter',
    kVersion: 0,
    ptrClass: MirrorServiceRemoterPtrAdapter,
    proxyClass: media.mojom.MirrorServiceRemoter.proxyClass,
    stubClass: MirrorServiceRemoterStubAdapter,
    validateRequest: media.mojom.MirrorServiceRemoter.validateRequest,
    validateResponse: media.mojom.MirrorServiceRemoter.validateResponse,
};

/**
 * Adapter for media.mojom.MirrorServiceRemotingSourcePtr.
 * @constructor
 */
function MirrorServiceRemotingSourcePtrAdapter(handleOrPtrInfo) {
  this.ptr = new mojo.InterfacePtrController(MirrorServiceRemotingSourceAdapter,
                                             handleOrPtrInfo);
}

MirrorServiceRemotingSourcePtrAdapter.prototype =
    Object.create(media.mojom.MirrorServiceRemotingSourcePtr.prototype);
MirrorServiceRemotingSourcePtrAdapter.prototype.constructor =
    MirrorServiceRemotingSourcePtrAdapter;

MirrorServiceRemotingSourcePtrAdapter.prototype.onSinkAvailable =
    function(metadata) {
  return this.ptr.getProxy().onSinkAvailable(metadata.toNewVersion());
};

/**
 * Adapter for media.mojom.MirrorServiceRemotingSource.
 */
var MirrorServiceRemotingSourceAdapter = {
    name: 'media.mojom.MirrorServiceRemotingSource',
    kVersion: 0,
    ptrClass: MirrorServiceRemotingSourcePtrAdapter,
    proxyClass: media.mojom.MirrorServiceRemotingSource.proxyClass,
    stubClass: null,
    validateRequest: media.mojom.MirrorServiceRemotingSource.validateRequest,
    validateResponse: null,
};

/**
 * Adapter for mediaRouter.mojom.MediaStatusObserver.
 * @constructor
 */
function MediaStatusObserverPtrAdapter(handleOrPtrInfo) {
  this.ptr = new mojo.InterfacePtrController(MediaStatusObserverAdapter,
                                             handleOrPtrInfo);
}

MediaStatusObserverPtrAdapter.prototype =
    Object.create(mediaRouter.mojom.MediaStatusObserverPtr.prototype);
MediaStatusObserverPtrAdapter.prototype.constructor =
    MediaStatusObserverPtrAdapter;

MediaStatusObserverPtrAdapter.prototype.onMediaStatusUpdated =
    function(status) {
  return this.ptr.getProxy().onMediaStatusUpdated(status.toNewVersion());
};

/**
 * Adapter for mediaRouter.mojom.MediaStatusObserver.
 */
var MediaStatusObserverAdapter = {
  name: 'mediaRouter.mojom.MediaStatusObserver',
  kVersion: 0,
  ptrClass: MediaStatusObserverPtrAdapter,
  proxyClass: mediaRouter.mojom.MediaStatusObserver.proxyClass,
  stubClass: null,
  validateRequest: mediaRouter.mojom.MediaStatusObserver.validateRequest,
  validateResponse: null,
};

/**
 * Converts a media sink to a MediaSink Mojo object.
 * @param {!MediaSink} sink A media sink.
 * @return {!mediaRouter.mojom.MediaSink} A Mojo MediaSink object.
 */
function sinkToMojo_(sink) {
  return new mediaRouter.mojom.MediaSink({
    'name': sink.friendlyName,
    'description': sink.description,
    'domain': sink.domain,
    'sinkId': sink.id,
    'iconType': sinkIconTypeToMojo(sink.iconType),
    'providerId': mediaRouter.mojom.MediaRouteProvider.Id.EXTENSION,
  });
}

/**
 * Converts a media sink's icon type to a MediaSink.IconType Mojo object.
 * @param {!MediaSink.IconType} type A media sink's icon type.
 * @return {!mediaRouter.mojom.MediaSink.IconType} A Mojo MediaSink.IconType
 *     object.
 */
function sinkIconTypeToMojo(type) {
  switch (type) {
    case 'cast':
      return mediaRouter.mojom.SinkIconType.CAST;
    case 'cast_audio_group':
      return mediaRouter.mojom.SinkIconType.CAST_AUDIO_GROUP;
    case 'cast_audio':
      return mediaRouter.mojom.SinkIconType.CAST_AUDIO;
    case 'meeting':
      return mediaRouter.mojom.SinkIconType.MEETING;
    case 'hangout':
      return mediaRouter.mojom.SinkIconType.HANGOUT;
    case 'education':
      return mediaRouter.mojom.SinkIconType.EDUCATION;
    case 'generic':
      return mediaRouter.mojom.SinkIconType.GENERIC;
    default:
      console.error('Unknown sink icon type : ' + type);
      return mediaRouter.mojom.SinkIconType.GENERIC;
  }
}

/**
 * Returns a Mojo MediaRoute object given a MediaRoute and a
 * media sink name.
 * @param {!MediaRoute} route
 * @return {!mediaRouter.mojom.MediaRoute}
 */
function routeToMojo_(route) {
  return new mediaRouter.mojom.MediaRoute({
    'mediaRouteId': route.id,
    'mediaSource': route.mediaSource,
    'mediaSinkId': route.sinkId,
    'description': route.description,
    'iconUrl': route.iconUrl,
    'isLocal': route.isLocal,
    'forDisplay': route.forDisplay,
    'isIncognito': route.offTheRecord,
    'isLocalPresentation': route.isOffscreenPresentation,
    'controllerType': route.controllerType,
    // Begin newly added properties, followed by the milestone they were
    // added.  The guard should be safe to remove N+2 milestones later.
    'presentationId': route.presentationId || ''  // M64
  });
}

/**
 * Converts a route message to a RouteMessage Mojo object.
 * @param {!RouteMessage} message
 * @return {!mediaRouter.mojom.RouteMessage} A Mojo RouteMessage object.
 */
function messageToMojo_(message) {
  if ("string" == typeof message.message) {
    return new mediaRouter.mojom.RouteMessage({
      'type': mediaRouter.mojom.RouteMessage.Type.TEXT,
      'message': message.message,
    });
  } else {
    return new mediaRouter.mojom.RouteMessage({
      'type': mediaRouter.mojom.RouteMessage.Type.BINARY,
      'data': message.message,
    });
  }
}

/**
 * Converts presentation connection state to Mojo enum value.
 * @param {!string} state
 * @return {!mediaRouter.mojom.MediaRouter.PresentationConnectionState}
 */
function presentationConnectionStateToMojo_(state) {
  var PresentationConnectionState =
      mediaRouter.mojom.MediaRouter.PresentationConnectionState;
  switch (state) {
    case 'connecting':
      return PresentationConnectionState.CONNECTING;
    case 'connected':
      return PresentationConnectionState.CONNECTED;
    case 'closed':
      return PresentationConnectionState.CLOSED;
    case 'terminated':
      return PresentationConnectionState.TERMINATED;
    default:
      console.error('Unknown presentation connection state: ' + state);
      return PresentationConnectionState.TERMINATED;
  }
}

/**
 * Converts presentation connection close reason to Mojo enum value.
 * @param {!string} reason
 * @return {!mediaRouter.mojom.MediaRouter.PresentationConnectionCloseReason}
 */
function presentationConnectionCloseReasonToMojo_(reason) {
  var PresentationConnectionCloseReason =
      mediaRouter.mojom.MediaRouter.PresentationConnectionCloseReason;
  switch (reason) {
    case 'error':
      return PresentationConnectionCloseReason.CONNECTION_ERROR;
    case 'closed':
      return PresentationConnectionCloseReason.CLOSED;
    case 'went_away':
      return PresentationConnectionCloseReason.WENT_AWAY;
    default:
      console.error('Unknown presentation connection close reason : ' +
          reason);
      return PresentationConnectionCloseReason.CONNECTION_ERROR;
  }
}

/**
 * Converts string to Mojo origin.
 * @param {string|!url.mojom.Origin} origin
 * @return {!url.mojom.Origin}
 */
function stringToMojoOrigin_(origin) {
  if (origin instanceof url.mojom.Origin) {
    return origin;
  }
  var originUrl = new URL(origin);
  var mojoOrigin = {};
  mojoOrigin.scheme = originUrl.protocol.replace(':', '');
  mojoOrigin.host = originUrl.hostname;
  var port = originUrl.port ? Number.parseInt(originUrl.port) : 0;
  switch (mojoOrigin.scheme) {
    case 'http':
      mojoOrigin.port = port || 80;
      break;
    case 'https':
      mojoOrigin.port = port || 443;
      break;
    default:
      throw new Error('Scheme must be http or https');
  }
  mojoOrigin.suborigin = '';
  return new url.mojom.Origin(mojoOrigin);
}

/**
 * Parses the given route request Error object and converts it to the
 * corresponding result code.
 * @param {!Error} error
 * @return {!mediaRouter.mojom.RouteRequestResultCode}
 */
function getRouteRequestResultCode_(error) {
  return error.errorCode ? error.errorCode :
    mediaRouter.mojom.RouteRequestResultCode.UNKNOWN_ERROR;
}

/**
 * Creates and returns a successful route response from given route.
 * @param {!MediaRoute} route
 * @return {!Object}
 */
function toSuccessRouteResponse_(route) {
  return {
      route: routeToMojo_(route),
      resultCode: mediaRouter.mojom.RouteRequestResultCode.OK
  };
}

/**
 * Creates and returns a error route response from given Error object.
 * @param {!Error} error
 * @return {!Object}
 */
function toErrorRouteResponse_(error) {
  return {
      errorText: error.message,
      resultCode: getRouteRequestResultCode_(error)
  };
}

/**
 * Creates a new MediaRouter.
 * Converts a route struct to its Mojo form.
 * @param {!mediaRouter.mojom.MediaRouterPtr} service
 * @constructor
 */
function MediaRouter(service) {
  /**
   * The Mojo service proxy. Allows extension code to call methods that reside
   * in the browser.
   * @type {!mediaRouter.mojom.MediaRouterPtr}
   */
  this.service_ = service;

  /**
   * The provider manager service delegate. Its methods are called by the
   * browser-resident Mojo service.
   * @type {!MediaRouter}
   */
  this.mrpm_ = new MediaRouteProvider(this);

  /**
   * Handle to a KeepAlive service object, which prevents the extension from
   * being suspended as long as it remains in scope.
   * @type {boolean}
   */
  this.keepAlive_ = null;

  /**
   * The bindings to bind the service delegate to the Mojo interface.
   * Object must remain in scope for the lifetime of the connection to
   * prevent the connection from closing automatically.
   * @type {!mojo.Binding}
   */
  this.mediaRouteProviderBinding_ = new mojo.Binding(
      mediaRouter.mojom.MediaRouteProvider, this.mrpm_);
}

/**
 * Returns definitions of Mojo core and generated Mojom classes that can be
 * used directly by the component.
 * @return {!Object}
 * TODO(imcheng): We should export these along with MediaRouter. This requires
 * us to modify the component to handle multiple exports. When that logic is
 * baked in for a couple of milestones, we should be able to remove this
 * method.
 * TODO(imcheng): We should stop exporting mojo bindings classes that the
 * Media Router extension doesn't directly use, such as
 * mojo.AssociatedInterfacePtrInfo, mojo.InterfacePtrController and
 * mojo.interfaceControl.
 */
MediaRouter.prototype.getMojoExports = function() {
  return {
    AssociatedInterfacePtrInfo: mojo.AssociatedInterfacePtrInfo,
    Binding: mojo.Binding,
    DialMediaSink: DialMediaSinkAdapter,
    CastMediaSink: CastMediaSinkAdapter,
    HangoutsMediaRouteController:
        mediaRouter.mojom.HangoutsMediaRouteController,
    HangoutsMediaStatusExtraData: HangoutsMediaStatusExtraDataAdapter,
    IPAddress: IPAddressAdapter,
    IPEndpoint: IPEndpointAdapter,
    InterfacePtrController: mojo.InterfacePtrController,
    InterfacePtrInfo: mojo.InterfacePtrInfo,
    InterfaceRequest: mojo.InterfaceRequest,
    MediaController: mediaRouter.mojom.MediaController,
    MediaStatus: MediaStatusAdapter,
    MediaStatusObserverPtr: mediaRouter.mojom.MediaStatusObserverPtr,
    MirroringCastMessage: mirroring.mojom.CastMessage,
    MirroringCastMessageChannel: mirroring.mojom.CastMessageChannel,
    MirroringCastMessageChannelPtr: mirroring.mojom.CastMessageChannelPtr,
    MirroringServiceHostPtr: mirroring.mojom.MirroringServiceHostPtr,
    MirroringSessionError: mirroring.mojom.SessionError,
    MirroringSessionObserver: mirroring.mojom.SessionObserver,
    MirroringSessionObserverPtr: mirroring.mojom.SessionObserverPtr,
    MirroringSessionParameters: mirroring.mojom.SessionParameters,
    MirroringSessionType: mirroring.mojom.SessionType,
    MirroringRemotingNamespace: mirroring.mojom.kRemotingNamespace,
    MirroringWebRtcNamespace: mirroring.mojom.kWebRtcNamespace,
    MirrorServiceRemoter: MirrorServiceRemoterAdapter,
    MirrorServiceRemoterPtr: MirrorServiceRemoterPtrAdapter,
    MirrorServiceRemotingSourcePtr: MirrorServiceRemotingSourcePtrAdapter,
    RemotingStopReason: media.mojom.RemotingStopReason,
    RemotingStartFailReason: media.mojom.RemotingStartFailReason,
    RemotingSinkFeature: media.mojom.RemotingSinkFeature,
    RemotingSinkAudioCapability:
        media.mojom.RemotingSinkAudioCapability,
    RemotingSinkVideoCapability:
        media.mojom.RemotingSinkVideoCapability,
    RemotingSinkMetadata: RemotingSinkMetadataAdapter,
    RouteControllerType: mediaRouter.mojom.RouteControllerType,
    Origin: url.mojom.Origin,
    Sink: MediaSinkAdapter,
    SinkExtraData: MediaSinkExtraDataAdapter,
    TimeDelta: mojoBase.mojom.TimeDelta,
    Url: url.mojom.Url,
    interfaceControl: mojo.interfaceControl,
    makeRequest: mojo.makeRequest,
  };
};

/**
 * Registers the Media Router Provider Manager with the Media Router.
 * @return {!Promise<Object>} Instance ID and config for the Media Router.
 */
MediaRouter.prototype.start = function() {
  return this.service_.registerMediaRouteProvider(
      mediaRouter.mojom.MediaRouteProvider.Id.EXTENSION,
      this.mediaRouteProviderBinding_.createInterfacePtrAndBind()).then(
          function(response) {
            return {
              'instance_id': response.instanceId,
              'config': {
                'enable_dial_discovery': response.config.enableDialDiscovery,
                'enable_cast_discovery': response.config.enableCastDiscovery,
                'enable_dial_sink_query': response.config.enableDialSinkQuery,
                'enable_cast_sink_query': response.config.enableCastSinkQuery,
                'use_views_dialog': response.config.useViewsDialog,
                'use_mirroring_service': response.config.useMirroringService,
              }
            };
          });
}

/**
 * Sets the service delegate methods.
 * @param {Object} handlers
 */
MediaRouter.prototype.setHandlers = function(handlers) {
  this.mrpm_.setHandlers(handlers);
}

/**
 * The keep alive status.
 * @return {boolean}
 */
MediaRouter.prototype.getKeepAlive = function() {
  return this.keepAlive_ != null;
};

/**
 * Called by the provider manager when a sink list for a given source is
 * updated.
 * @param {!string} sourceUrn
 * @param {!Array<!MediaSink>} sinks
 * @param {!Array<string|!url.mojom.Origin>} origins
 */
MediaRouter.prototype.onSinksReceived = function(sourceUrn, sinks, origins) {
  // |origins| is a string array if the Media Router component extension version
  // is 59 or older. Without the stringToMojoOrigin_() conversion, clients using
  // those extension versions would see a crash shown in
  // https://crbug.com/787427.
  this.service_.onSinksReceived(
      mediaRouter.mojom.MediaRouteProvider.Id.EXTENSION, sourceUrn,
      sinks.map(sinkToMojo_), origins.map(stringToMojoOrigin_));
};

/**
 * Called by the provider manager when a sink is found to notify the MR of the
 * sink's ID. The actual sink will be returned through the normal sink list
 * update process, so this helps the MR identify the search result in the
 * list.
 * @param {string} pseudoSinkId  ID of the pseudo sink that started the
 *     search.
 * @param {string} sinkId ID of the newly-found sink.
 */
MediaRouter.prototype.onSearchSinkIdReceived = function(
    pseudoSinkId, sinkId) {
  this.service_.onSearchSinkIdReceived(pseudoSinkId, sinkId);
};

/**
 * Called by the provider manager to keep the extension from suspending
 * if it enters a state where suspension is undesirable (e.g. there is an
 * active MediaRoute.)
 * If keepAlive is true, the extension is kept alive.
 * If keepAlive is false, the extension is allowed to suspend.
 * @param {boolean} keepAlive
 */
MediaRouter.prototype.setKeepAlive = function(keepAlive) {
  if (keepAlive === false && this.keepAlive_) {
    this.keepAlive_.ptr.reset();
    this.keepAlive_ = null;
  } else if (keepAlive === true && !this.keepAlive_) {
    this.keepAlive_ = new extensions.KeepAlivePtr;
    Mojo.bindInterface(extensions.KeepAlive.name,
                       mojo.makeRequest(this.keepAlive_).handle);
  }
};

/**
 * Called by the provider manager to send an issue from a media route
 * provider to the Media Router, to show the user.
 * @param {!Object} issue The issue object.
 */
MediaRouter.prototype.onIssue = function(issue) {
  function issueSeverityToMojo_(severity) {
    switch (severity) {
      case 'fatal':
        return mediaRouter.mojom.Issue.Severity.FATAL;
      case 'warning':
        return mediaRouter.mojom.Issue.Severity.WARNING;
      case 'notification':
        return mediaRouter.mojom.Issue.Severity.NOTIFICATION;
      default:
        console.error('Unknown issue severity: ' + severity);
        return mediaRouter.mojom.Issue.Severity.NOTIFICATION;
    }
  }

  function issueActionToMojo_(action) {
    switch (action) {
      case 'dismiss':
        return mediaRouter.mojom.Issue.ActionType.DISMISS;
      case 'learn_more':
        return mediaRouter.mojom.Issue.ActionType.LEARN_MORE;
      default:
        console.error('Unknown issue action type : ' + action);
        return mediaRouter.mojom.Issue.ActionType.OK;
    }
  }

  var secondaryActions = (issue.secondaryActions || []).map(issueActionToMojo_);
  this.service_.onIssue(new mediaRouter.mojom.Issue({
    'routeId': issue.routeId || '',
    'severity': issueSeverityToMojo_(issue.severity),
    'title': issue.title,
    'message': issue.message || '',
    'defaultAction': issueActionToMojo_(issue.defaultAction),
    'secondaryActions': secondaryActions,
    'helpPageId': issue.helpPageId,
    'isBlocking': issue.isBlocking,
    'sinkId': issue.sinkId || ''
  }));
};

/**
 * Called by the provider manager when the set of active routes
 * has been updated.
 * @param {!Array<MediaRoute>} routes The active set of media routes.
 * @param {string=} sourceUrn The sourceUrn associated with this route
 *     query.
 * @param {Array<string>=} joinableRouteIds The active set of joinable
 *     media routes.
 */
MediaRouter.prototype.onRoutesUpdated = function(
    routes, sourceUrn = '', joinableRouteIds = []) {
  this.service_.onRoutesUpdated(
      mediaRouter.mojom.MediaRouteProvider.Id.EXTENSION,
      routes.map(routeToMojo_), sourceUrn, joinableRouteIds);
};

/**
 * Called by the provider manager when sink availability has been updated.
 * @param {!mediaRouter.mojom.MediaRouter.SinkAvailability} availability
 *     The new sink availability.
 */
MediaRouter.prototype.onSinkAvailabilityUpdated = function(availability) {
  this.service_.onSinkAvailabilityUpdated(
      mediaRouter.mojom.MediaRouteProvider.Id.EXTENSION, availability);
};

/**
 * Called by the provider manager when the state of a presentation connected
 * to a route has changed.
 * @param {string} routeId
 * @param {string} state
 */
MediaRouter.prototype.onPresentationConnectionStateChanged =
    function(routeId, state) {
  this.service_.onPresentationConnectionStateChanged(
      routeId, presentationConnectionStateToMojo_(state));
};

/**
 * Called by the provider manager when the state of a presentation connected
 * to a route has closed.
 * @param {string} routeId
 * @param {string} reason
 * @param {string} message
 */
MediaRouter.prototype.onPresentationConnectionClosed =
    function(routeId, reason, message) {
  this.service_.onPresentationConnectionClosed(
      routeId, presentationConnectionCloseReasonToMojo_(reason), message);
};

/**
 * @param {string} routeId
 * @param {!Array<!RouteMessage>} mesages
 */
MediaRouter.prototype.onRouteMessagesReceived = function(routeId, messages) {
  this.service_.onRouteMessagesReceived(
      routeId, messages.map(messageToMojo_));
};

/**
 * @param {number} tabId
 * @param {!media.mojom.MirrorServiceRemoterPtr} remoter
 * @param {!mojo.InterfaceRequest} remotingSource
 */
MediaRouter.prototype.onMediaRemoterCreated = function(tabId, remoter,
    remotingSource) {
  this.service_.onMediaRemoterCreated(
      tabId,
      new media.mojom.MirrorServiceRemoterPtr(remoter.ptr.passInterface()),
      remotingSource);
}

/**
 * Returns current status of media sink service in JSON format.
 * @return {!Promise<!{status: string}>}
 */
MediaRouter.prototype.getMediaSinkServiceStatus = function() {
  return this.service_.getMediaSinkServiceStatus();
}

/**
 * @param {int32} target_tab_id
 * @param {!mojo.InterfaceRequest} request
 */
MediaRouter.prototype.getMirroringServiceHostForTab = function(
    target_tab_id, request) {
  this.service_.getMirroringServiceHostForTab(target_tab_id, request);
}

/**
 * @param {int32} initiator_tab_id
 * @param {!string} desktop_stream_id
 * @param {!mojo.InterfaceRequest} request
 */
MediaRouter.prototype.getMirroringServiceHostForDesktop = function(
    initiator_tab_id, desktop_stream_id, request) {
  this.service_.getMirroringServiceHostForDesktop(initiator_tab_id,
      desktop_stream_id, request);
}

/**
 * @param {!url.mojom.Url} presentation_url
 * @param {!string} presentation_id
 * @param {!mojo.InterfaceRequest} request
 */
MediaRouter.prototype.getMirroringServiceHostForOffscreenTab = function(
    presentation_url, presentation_id, request) {
  this.service_.getMirroringServiceHostForOffscreenTab(presentation_url,
      presentation_id, request);
}

/**
 * Object containing callbacks set by the provider manager.
 *
 * @constructor
 * @struct
 */
function MediaRouterHandlers() {
  /**
   * @type {function(!string, !string, !string, !string, !number)}
   */
  this.createRoute = null;

  /**
   * @type {function(!string, !string, !string, !number)}
   */
  this.joinRoute = null;

  /**
   * @type {function(string): Promise}
   */
  this.terminateRoute = null;

  /**
   * @type {function(string)}
   */
  this.startObservingMediaSinks = null;

  /**
   * @type {function(string)}
   */
  this.stopObservingMediaSinks = null;

  /**
   * @type {function(string, string): Promise}
   */
  this.sendRouteMessage = null;

  /**
   * @type {function(string, Uint8Array): Promise}
   */
  this.sendRouteBinaryMessage = null;

  /**
   * @type {function(string)}
   */
  this.startListeningForRouteMessages = null;

  /**
   * @type {function(string)}
   */
  this.stopListeningForRouteMessages = null;

  /**
   * @type {function(string)}
   */
  this.detachRoute = null;

  /**
   * @type {function()}
   */
  this.startObservingMediaRoutes = null;

  /**
   * @type {function()}
   */
  this.stopObservingMediaRoutes = null;

  /**
   * @type {function()}
   */
  this.connectRouteByRouteId = null;

  /**
   * @type {function()}
   */
  this.enableMdnsDiscovery = null;

  /**
   * @type {function()}
   */
  this.updateMediaSinks = null;

  /**
   * @type {function(string, string, !SinkSearchCriteria): string}
   */
  this.searchSinks = null;

  /**
   * @type {function()}
   */
  this.provideSinks = null;

  /**
   * @type {function(string, !mojo.InterfaceRequest,
   *            !mediaRouter.mojom.MediaStatusObserverPtr): !Promise<void>}
   */
  this.createMediaRouteController = null;
};

/**
 * Routes calls from Media Router to the provider manager extension.
 * Registered with the MediaRouter stub.
 * @param {!MediaRouter} MediaRouter proxy to call into the
 * Media Router mojo interface.
 * @constructor
 */
function MediaRouteProvider(mediaRouter) {
  /**
   * Object containing JS callbacks into Provider Manager code.
   * @type {!MediaRouterHandlers}
   */
  this.handlers_ = new MediaRouterHandlers();

  /**
   * Proxy class to the browser's Media Router Mojo service.
   * @type {!MediaRouter}
   */
  this.mediaRouter_ = mediaRouter;
}

/*
 * Sets the callback handler used to invoke methods in the provider manager.
 *
 * @param {!MediaRouterHandlers} handlers
 */
MediaRouteProvider.prototype.setHandlers = function(handlers) {
  this.handlers_ = handlers;
  var requiredHandlers = [
    'stopObservingMediaRoutes',
    'startObservingMediaRoutes',
    'sendRouteMessage',
    'sendRouteBinaryMessage',
    'startListeningForRouteMessages',
    'stopListeningForRouteMessages',
    'detachRoute',
    'terminateRoute',
    'joinRoute',
    'createRoute',
    'stopObservingMediaSinks',
    'startObservingMediaRoutes',
    'connectRouteByRouteId',
    'enableMdnsDiscovery',
    'updateMediaSinks',
    'searchSinks',
    'provideSinks',
    'createMediaRouteController',
    'onBeforeInvokeHandler'
  ];
  requiredHandlers.forEach(function(nextHandler) {
    if (handlers[nextHandler] === undefined) {
      console.error(nextHandler + ' handler not registered.');
    }
  });
}

/**
 * Starts querying for sinks capable of displaying the media source
 * designated by |sourceUrn|.  Results are returned by calling
 * OnSinksReceived.
 * @param {!string} sourceUrn
 */
MediaRouteProvider.prototype.startObservingMediaSinks =
    function(sourceUrn) {
  this.handlers_.onBeforeInvokeHandler();
  this.handlers_.startObservingMediaSinks(sourceUrn);
};

/**
 * Stops querying for sinks capable of displaying |sourceUrn|.
 * @param {!string} sourceUrn
 */
MediaRouteProvider.prototype.stopObservingMediaSinks =
    function(sourceUrn) {
  this.handlers_.onBeforeInvokeHandler();
  this.handlers_.stopObservingMediaSinks(sourceUrn);
};

/**
 * Requests that |sinkId| render the media referenced by |sourceUrn|. If the
 * request is from the Presentation API, then origin and tabId will
 * be populated.
 * @param {!string} sourceUrn Media source to render.
 * @param {!string} sinkId Media sink ID.
 * @param {!string} presentationId Presentation ID from the site
 *     requesting presentation. TODO(mfoltz): Remove.
 * @param {!url.mojom.Origin} origin Origin of site requesting presentation.
 * @param {!number} tabId ID of tab requesting presentation.
 * @param {!mojo_base.mojom.TimeDelta} timeout If positive, the timeout
 *     duration for the request. Otherwise, the default duration will be used.
 * @param {!boolean} incognito If true, the route is being requested by
 *     an incognito profile.
 * @return {!Promise.<!Object>} A Promise resolving to an object describing
 *     the newly created media route, or rejecting with an error message on
 *     failure.
 */
MediaRouteProvider.prototype.createRoute =
    function(sourceUrn, sinkId, presentationId, origin, tabId,
             timeout, incognito) {
  this.handlers_.onBeforeInvokeHandler();
  return this.handlers_.createRoute(
      sourceUrn, sinkId, presentationId, origin, tabId,
      Math.floor(timeout.microseconds / 1000), incognito)
      .then(function(route) {
        return toSuccessRouteResponse_(route);
      },
      function(err) {
        return toErrorRouteResponse_(err);
      });
};

/**
 * Handles a request via the Presentation API to join an existing route given
 * by |sourceUrn| and |presentationId|. |origin| and |tabId| are used for
 * validating same-origin/tab scope.
 * @param {!string} sourceUrn Media source to render.
 * @param {!string} presentationId Presentation ID to join.
 * @param {!url.mojom.Origin} origin Origin of site requesting join.
 * @param {!number} tabId ID of tab requesting join.
 * @param {!mojo_base.mojom.TimeDelta} timeout If positive, the timeout
 *     duration for the request. Otherwise, the default duration will be used.
 * @param {!boolean} incognito If true, the route is being requested by
 *     an incognito profile.
 * @return {!Promise.<!Object>} A Promise resolving to an object describing
 *     the newly created media route, or rejecting with an error message on
 *     failure.
 */
MediaRouteProvider.prototype.joinRoute =
    function(sourceUrn, presentationId, origin, tabId, timeout,
             incognito) {
  this.handlers_.onBeforeInvokeHandler();
  return this.handlers_.joinRoute(
      sourceUrn, presentationId, origin, tabId,
      Math.floor(timeout.microseconds / 1000), incognito)
      .then(function(route) {
        return toSuccessRouteResponse_(route);
      },
      function(err) {
        return toErrorRouteResponse_(err);
      });
};

/**
 * Handles a request via the Presentation API to join an existing route given
 * by |sourceUrn| and |routeId|. |origin| and |tabId| are used for
 * validating same-origin/tab scope.
 * @param {!string} sourceUrn Media source to render.
 * @param {!string} routeId Route ID to join.
 * @param {!string} presentationId Presentation ID to join.
 * @param {!url.mojom.Origin} origin Origin of site requesting join.
 * @param {!number} tabId ID of tab requesting join.
 * @param {!mojo_base.mojom.TimeDelta} timeout If positive, the timeout
 *     duration for the request. Otherwise, the default duration will be used.
 * @param {!boolean} incognito If true, the route is being requested by
 *     an incognito profile.
 * @return {!Promise.<!Object>} A Promise resolving to an object describing
 *     the newly created media route, or rejecting with an error message on
 *     failure.
 */
MediaRouteProvider.prototype.connectRouteByRouteId =
    function(sourceUrn, routeId, presentationId, origin, tabId,
             timeout, incognito) {
  this.handlers_.onBeforeInvokeHandler();
  return this.handlers_.connectRouteByRouteId(
      sourceUrn, routeId, presentationId, origin, tabId,
      Math.floor(timeout.microseconds / 1000), incognito)
      .then(function(route) {
        return toSuccessRouteResponse_(route);
      },
      function(err) {
        return toErrorRouteResponse_(err);
      });
};

/**
 * Terminates the route specified by |routeId|.
 * @param {!string} routeId
 * @return {!Promise<!Object>} A Promise resolving to an object describing
 *    the result of the terminate operation, or rejecting with an error
 *    message and code if the operation failed.
 */
MediaRouteProvider.prototype.terminateRoute = function(routeId) {
  this.handlers_.onBeforeInvokeHandler();
  return this.handlers_.terminateRoute(routeId).then(
      () => ({resultCode: mediaRouter.mojom.RouteRequestResultCode.OK}),
      (err) => toErrorRouteResponse_(err));
};

/**
 * Posts a message to the route designated by |routeId|.
 * @param {!string} routeId
 * @param {!string} message
 */
MediaRouteProvider.prototype.sendRouteMessage = function(
  routeId, message) {
  this.handlers_.onBeforeInvokeHandler();
  this.handlers_.sendRouteMessage(routeId, message);
};

/**
 * Sends a binary message to the route designated by |routeId|.
 * @param {!string} routeId
 * @param {!Array<number>} data
 */
MediaRouteProvider.prototype.sendRouteBinaryMessage = function(
  routeId, data) {
  this.handlers_.onBeforeInvokeHandler();
  this.handlers_.sendRouteBinaryMessage(routeId, new Uint8Array(data));
};

/**
 * Listen for messages from a route.
 * @param {!string} routeId
 */
MediaRouteProvider.prototype.startListeningForRouteMessages = function(
    routeId) {
  this.handlers_.onBeforeInvokeHandler();
  this.handlers_.startListeningForRouteMessages(routeId);
};

/**
 * @param {!string} routeId
 */
MediaRouteProvider.prototype.stopListeningForRouteMessages = function(
    routeId) {
  this.handlers_.onBeforeInvokeHandler();
  this.handlers_.stopListeningForRouteMessages(routeId);
};

/**
 * Indicates that the presentation connection that was connected to |routeId|
 * is no longer connected to it.
 * @param {!string} routeId
 */
MediaRouteProvider.prototype.detachRoute = function(
    routeId) {
  this.handlers_.detachRoute(routeId);
};

/**
 * Requests that the provider manager start sending information about active
 * media routes to the Media Router.
 * @param {!string} sourceUrn
 */
MediaRouteProvider.prototype.startObservingMediaRoutes = function(sourceUrn) {
  this.handlers_.onBeforeInvokeHandler();
  this.handlers_.startObservingMediaRoutes(sourceUrn);
};

/**
 * Requests that the provider manager stop sending information about active
 * media routes to the Media Router.
 * @param {!string} sourceUrn
 */
MediaRouteProvider.prototype.stopObservingMediaRoutes = function(sourceUrn) {
  this.handlers_.onBeforeInvokeHandler();
  this.handlers_.stopObservingMediaRoutes(sourceUrn);
};

/**
 * Enables mDNS device discovery.
 */
MediaRouteProvider.prototype.enableMdnsDiscovery = function() {
  this.handlers_.onBeforeInvokeHandler();
  this.handlers_.enableMdnsDiscovery();
};

/**
 * Requests that the provider manager update media sinks.
 * @param {!string} sourceUrn
 */
MediaRouteProvider.prototype.updateMediaSinks = function(sourceUrn) {
  this.handlers_.onBeforeInvokeHandler();
  this.handlers_.updateMediaSinks(sourceUrn);
};

/**
 * Requests that the provider manager search its providers for a sink matching
 * |searchCriteria| that is compatible with |sourceUrn|. If a sink is found
 * that can be used immediately for route creation, its ID is returned.
 * Otherwise the empty string is returned.
 *
 * @param {string} sinkId Sink ID of the pseudo sink generating the request.
 * @param {string} sourceUrn Media source to be used with the sink.
 * @param {!SinkSearchCriteria} searchCriteria Search criteria for the route
 *     providers.
 * @return {!Promise.<!{sink_id: !string}>} A Promise resolving to either the
 *     sink ID of the sink found by the search that can be used for route
 *     creation, or the empty string if no route can be immediately created.
 */
MediaRouteProvider.prototype.searchSinks = function(
    sinkId, sourceUrn, searchCriteria) {
  this.handlers_.onBeforeInvokeHandler();
 return this.handlers_.searchSinks(sinkId, sourceUrn, searchCriteria).then(
      sinkId => {
        return { 'sinkId': sinkId };
      },
      () => {
        return { 'sinkId': '' };
      });
};

/**
 * Notifies the provider manager that MediaRouter has discovered a list of
 * sinks.
 * @param {string} providerName
 * @param {!Array<!mediaRouter.mojom.MediaSink>} sinks
 */
MediaRouteProvider.prototype.provideSinks = function(providerName, sinks) {
  this.handlers_.onBeforeInvokeHandler();
  this.handlers_.provideSinks(providerName,
                              sinks.map(MediaSinkAdapter.fromNewVersion));
};

/**
 * Creates a controller for the given route and binds the given
 * InterfaceRequest to it, and registers an observer for media status updates
 * for the route.
 * @param {string} routeId
 * @param {!mojo.InterfaceRequest} controllerRequest
 * @param {!mediaRouter.mojom.MediaStatusObserverPtr} observer
 * @return {!Promise<!{success: boolean}>} Resolves to true if a controller
 *     is created. Resolves to false if a controller cannot be created, or if
 *     the controller is already bound.
 */
MediaRouteProvider.prototype.createMediaRouteController = function(
    routeId, controllerRequest, observer) {
  this.handlers_.onBeforeInvokeHandler();
  return this.handlers_.createMediaRouteController(
      routeId, controllerRequest,
      new MediaStatusObserverPtrAdapter(observer.ptr.passInterface())).then(
          () => ({success: true}), e => ({success: false}));
};

var ptr = new mediaRouter.mojom.MediaRouterPtr;
Mojo.bindInterface(mediaRouter.mojom.MediaRouter.name,
                   mojo.makeRequest(ptr).handle);
exports.$set('returnValue', new MediaRouter(ptr));
