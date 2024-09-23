// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_COMMON_MEDIA_SOURCE_H_
#define COMPONENTS_MEDIA_ROUTER_COMMON_MEDIA_SOURCE_H_

#include <optional>
#include <ostream>
#include <string>

#include "base/hash/hash.h"
#include "url/gurl.h"

namespace media {
enum class AudioCodec;
enum class VideoCodec;
}  // namespace media

namespace media_router {

// URL schemes used by Presentation URLs for Cast and DIAL.
constexpr char kCastPresentationUrlScheme[] = "cast";
constexpr char kCastDialPresentationUrlScheme[] = "cast-dial";
constexpr char kDialPresentationUrlScheme[] = "dial";
constexpr char kRemotePlaybackPresentationUrlScheme[] = "remote-playback";

// URL prefix used by legacy Cast presentations.
constexpr char kLegacyCastPresentationUrlPrefix[] =
    "https://google.com/cast#__castAppId__=";

// Strings used in presentation IDs by the Cast SDK implementation.
// TODO(takumif): Move them out of this file, since they are not directly
// related to MediaSource.
//
// This value must be the same as |chrome.cast.AUTO_JOIN_PRESENTATION_ID| in the
// component extension.
constexpr char kAutoJoinPresentationId[] = "auto-join";
// This value must be the same as |chrome.cast.PRESENTATION_ID_PREFIX| in the
// component extension.
constexpr char kCastPresentationIdPrefix[] = "cast-session_";

// Returns true if |url| represents a legacy Cast presentation URL, i.e., it
// starts with |kLegacyCastPresentationUrlPrefix|.
bool IsLegacyCastPresentationUrl(const GURL& url);

// Returns true if |url| is a valid presentation URL.
bool IsValidPresentationUrl(const GURL& url);

// Returns true if |media_source| has a valid presentation URL.
bool IsValidStandardPresentationSource(const std::string& media_source);

// Returns true if |presentation_id| is an ID used by auto-join requests.
bool IsAutoJoinPresentationId(const std::string& presentation_id);

// A media source specifier, represented as an ID string and possibly a URL.
// If there is a URL, its string value with be the same as the ID string.
class MediaSource {
 public:
  using Id = std::string;

  // Create from an arbitrary string, which may or may not be a presentation
  // URL.
  explicit MediaSource(const MediaSource::Id& id);

  // Create from a URL which is assumed to be a presentation URL.  If
  // |presentation_url| is not a valid presentation URL, the resulting object's
  // IsValid() method will return false.
  explicit MediaSource(const GURL& presentation_url);

  ~MediaSource();

  // Gets the ID of the media source.
  const Id& id() const { return id_; }

  // If the source ID is a presentation URL, returns the URL; otherwise returns
  // an empty GURL.
  const GURL& url() const { return url_; }

  // Returns true if two MediaSource objects use the same media ID.
  bool operator==(const MediaSource& other) const { return id_ == other.id(); }

  bool operator<(const MediaSource& other) const { return id_ < other.id(); }

  // Compare operator for std::optional<MediaSource>
  struct Cmp {
    bool operator()(const std::optional<MediaSource>& source1,
                    const std::optional<MediaSource>& source2) const {
      Id id1 = (source1.has_value()) ? (source1->id()) : "";
      Id id2 = (source2.has_value()) ? (source2->id()) : "";
      return id1 < id2;
    }
  };

  // Protocol-specific media source object creation.
  // Returns MediaSource URI depending on the type of source.
  static MediaSource ForAnyTab();
  static MediaSource ForTab(int tab_id);
  static MediaSource ForPresentationUrl(const GURL& presentation_url);
  static MediaSource ForRemotePlayback(int tab_id,
                                       media::VideoCodec video_codec,
                                       media::AudioCodec audio_codec);

  // Creates a media source for a specific desktop.
  // `desktop_media_id` is the string representing content::DesktopMediaID.
  static MediaSource ForDesktop(const std::string& desktop_media_id,
                                bool with_audio);

  // Creates a media source representing a yet-to-be-chosen desktop, screen or
  // window. This must never be passed to a mojom::MediaRouteProvider
  // implementation's CreateRoute(). This is just a placeholder for the media
  // router UI until the moment the user initiates capture and picks a specific
  // desktop/screen/window.
  static MediaSource ForUnchosenDesktop();

  // Returns true if source outputs its content via tab mirroring.
  bool IsTabMirroringSource() const;

  // Returns true if source outputs its content via desktop mirroring.
  bool IsDesktopMirroringSource() const;

  // Returns true if this is represents a Cast Presentation URL.
  bool IsCastPresentationUrl() const;

  // Returns true if the source is a RemotePlayback source.
  bool IsRemotePlaybackSource() const;

  // Parses the ID and returns the SessionTabHelper tab ID referencing a source
  // tab.  Don't rely on this method returning something useful without first
  // calling IsTabMirroringSource(); Returns std::nullopt for non-tab sources
  // or the ForAnyTab() source.
  std::optional<int> TabId() const;

  // Parse the tab ID from the RemotePlayback source. Returns std::nullopt for
  // non-RemotePlayback sources or invalid formats.
  std::optional<int> TabIdFromRemotePlaybackSource() const;

  // When this source was created by ForDesktop(), returns the string
  // representing content::DesktopMediaID. Otherwise, returns std::nullopt.
  std::optional<std::string> DesktopStreamId() const;

  // Returns true if this source represents desktop capture that also provides
  // audio loopback capture. Returns false otherwise.
  bool IsDesktopSourceWithAudio() const;

  // Returns true this source outputs its content via DIAL.
  // TODO(crbug.com/41366226): Move this to in-browser DIAL/Cast MRP when we
  // have one.
  bool IsDialSource() const;

  // Returns empty string if this source is not DIAL media source, or is not a
  // valid DIAL media source.
  std::string AppNameFromDialSource() const;

  // Returns a shortened copy of the media source ID suitable for logging.
  std::string TruncateForLogging(size_t max_length) const;

  // Append the "&tab_id=xxx" string to the presentation url. The `tab_id` is
  // used for MediaDialogView to associate the local media session notification
  // with a Reote Playback MediaRoute so that users can control Remote Playback
  // session from GMC.
  // TODO(crbug.com/1491212): remove the `tab_id` field from the MediaSource
  // and use the MVC model in GMC to handle Remote Playback UI presentation
  // logic.
  void AppendTabIdToRemotePlaybackUrlQuery(int tab_id);

 private:
  MediaSource::Id id_;
  GURL url_;
};

// Only for debug logging.  This operator is defined inline so it doesn't add
// any code in release builds.  (Omitting the definition entirely when NDEBUG is
// defined causes linker errors on Android.)
inline std::ostream& operator<<(std::ostream& stream,
                                const MediaSource& source) {
  return stream << "MediaSource[" << source.id() << "]";
}

}  // namespace media_router

#endif  // COMPONENTS_MEDIA_ROUTER_COMMON_MEDIA_SOURCE_H_
