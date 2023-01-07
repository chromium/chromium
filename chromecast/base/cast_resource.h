// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BASE_CAST_RESOURCE_H_
#define CHROMECAST_BASE_CAST_RESOURCE_H_

namespace chromecast {

// A CastResource is a user of 1 or more Resources (primary screen, audio,
// etc). As a user, it is responsible for doing any cast-specific component
// initialization when the Resources it uses are granted. This initialization
// is referred to as "acquiring" the Resources. Conversely, when the Resources
// it uses are revoked, it must deinitialize these cast-specific components.
// This deinitialization is referred to as "releasing" the Resources.
// TODO(maclellant): RENAME/DESIGN THIS CLASS IN COMING REFACTOR.
class CastResource {
 public:
  // Resources necessary to run cast apps. CastResource may contain union of the
  // following types.
  // TODO(yucliu): Split video resources and graphic resources.
  enum Resource {
    kResourceNone = 0,
    // All resources necessary to render sounds, for example, audio pipeline,
    // speaker, etc.
    kResourceAudio = 1 << 0,
    // All resources necessary to render videos or images, for example, video
    // pipeline, primary graphics plane, display, etc.
    kResourceScreenPrimary = 1 << 1,
    // All resources necessary to render overlaid images, for example, secondary
    // graphics plane, LCD, etc.
    kResourceScreenSecondary = 1 << 2,
    // Collection of resources used for display only combined with bitwise or.
    kResourceDisplayOnly = (kResourceScreenPrimary | kResourceScreenSecondary),
    // Collection of all resources combined with bitwise or.
    kResourceAll =
        (kResourceAudio | kResourceScreenPrimary | kResourceScreenSecondary),
  };

  // A Client is responsible for notifying all registered CastResource's when
  // Resources are granted/revoked so that they can acquire/release those
  // Resources. When a CastResource is done acquiring/releasing, it responds
  // to the Client that it has completed. A Client can have multiple registered
  // CastResource's, but each CastResouce has 1 Client that it responds to.
  class Client {
   public:
    // Called to register a CastResource with a Client. After registering, a
    // CastResource will start getting notified when to acquire/release
    // Resources. The Client does not take ownership of |cast_resource|. It can
    // be called from any thread.
    virtual void RegisterCastResource(CastResource* cast_resource) = 0;

    // TODO(esum): Add OnResourceAcquired() here once AcquireResource is
    // allowed to be asynchronous.

    // Called when part or all resources are released. It can be called from any
    // thread.
    //   |cast_resource| the CastResource that is released. The pointer may be
    //                   invalid. Client can't call functions with that pointer.
    //   |remain| the unreleased resource of CastResource. If kResourceNone is
    //            returned, Client will remove the resource from its watching
    //            list.
    virtual void OnResourceReleased(CastResource* cast_resource,
                                    Resource remain) = 0;

   protected:
    virtual ~Client() {}
  };

  CastResource(const CastResource&) = delete;
  CastResource& operator=(const CastResource&) = delete;

  // Sets the Client for the CastResource to respond to when it is done with
  // Acquire/ReleaseResource.
  void SetCastResourceClient(Client* client);
  // Called to acquire resources after OEM has granted them, and before
  // they start getting used by consumers. Implementation must be synchronous
  // since consumers will start using the resource immediately afterwards.
  // TODO(esum): We should allow this method to be asynchronous in case an
  // implementer needs to make expensive calls and doesn't want to block the
  // UI thread (b/26239576). For now, don't do anything expensive in your
  // implementation; if you really need to, then this bug has to be resolved.
  virtual void AcquireResource(Resource resource) = 0;
  // Called to release resources. Implementation should call
  // Client::OnResourceReleased when resource is released on its side.
  virtual void ReleaseResource(Resource resource) = 0;

 protected:
  CastResource() : client_(nullptr) {}
  virtual ~CastResource() {}

  // For derived classes to register themselves with their Client through
  // Client::RegisterCastResource.
  void RegisterWithClient();
  void NotifyResourceReleased(Resource remain);

 private:
  Client* client_;
};

}  // namespace chromecast

#endif  // CHROMECAST_BASE_CAST_RESOURCE_H_
