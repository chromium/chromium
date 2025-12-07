# TabStripService API Developer Guide
This document provides a guide on extending the TabStripService API.
TabStripService is a stable, cross-platform interface for interacting with the
browser's tabstrip. This guide focuses on two extensions: adding new methods and
adding new events.

#### Other documentation
- [API Overview](tab_strip_api.md)
- [IPC Review](/docs/security/ipc-reviews.md)

## High-Level Architecture
The `TabStripService` is a service-oriented interface that decouples clients
from the browser's `TabStripModel`. The API contract is defined in .mojom files
located in `//chrome/browser/ui/tabs/tab_strip_api/`. Mojo is a
platform-agnostic IDL that facilitates communication across inter-process and
intra-process boundaries.

```
 [ API Contract (.mojom files) ]
    - Defines the service, data structures, and events.
          |
          V

 [ Service Implementation ]
  - Implements the .mojom contract.
  - Handles client connections and api operations.

      |                         |
      |----------uses-----------|
      |                         |
      V                         V

 [ Adapters ]               [ Events ]
  - Decouples service from   - Listens to browser changes.
    browser internals.       - Translates to Mojo events.
                             - Broadcasts to clients.

 [ Utilities ]
 - /converters: Translates between browser and mojo types.
 - /types: Classes that are typemapped to mojo struct.
```

## Adding a new method
TODO(crbug.com/409086859): Add a link to a sample CL.

How to add a new method that a client can invoke.

1. Define the interface

Define the method in the mojom file. This is the API contract between all
clients and services. All methods in TabStripApi will have a result type.

`//chrome/browser/ui/tabs/tab_strip_api/tab_strip_api.mojom`

```
interface TabStripService {
  [Sync]
  ReloadTab(NodeId tab_id)
      => result<mojo_base.mojom.Empty, mojo_base.mojom.Error>;
}
```
2. Update the adapter interface

The service uses an adapter pattern to decouple it from the TabStripModel. Add
the new method to the `TabStripModelAdapter` interface and its implementation.

```
//chrome/browser/ui/tabs/tab_strip_api/adapters/tab_strip_model_adapter.h
//chrome/browser/ui/tabs/tab_strip_api/adapters/tab_strip_model_adapter.cc
```

```
class TabStripModelAdapter {
  public:
    virtual void ReloadTab(tabs::TabHandle handle) = 0;
};

class TabStripModelAdapterImpl : public TabStripModelAdapter{
 public:
  void ReloadTab(tabs::TabHandle handle) override {
    handle.Get()->ReloadTab();
  }
}
```

3. Implement the service method

Implement the method in `TabStripServiceImpl`. This step is necessary for
validating all arguments before passing them to the adapter.

`//chrome/browser/ui/tabs/tab_strip_api/tab_strip_service_impl.cc`

```
void TabStripServiceImpl::ReloadTab(const tabs_api::NodeId& tab_mojom_id,
                                      ReloadTabCallback callback) {
  const std::optional<tabs::TabHandle> tab_handle = tab_mojom_id.ToTabHandle();
  if (!tab_handle.has_value()) {
    std::move(callback).Run(base::unexpected(mojo_base::mojom::Error::New(
        mojo_base::mojom::Code::kInvalidArgument, "Invalid tab ID")));
    return;
  }

  if (tab_strip_model_adapter_->ReloadTab(tab_handle.value())) {
    std::move(callback).Run(base::ok(mojo_base::mojom::Empty::New()));
  } else {
    std::move(callback).Run(base::unexpected(mojo_base::mojom::Error::New(
        mojo_base::mojom::Code::kNotFound, "Tab not found")));
  }
}
```

Note: Adding new methods to mojom will require an
[IPC Review](/docs/security/ipc-reviews.md).

## Adding a new event
How to add a new event that clients can listen for.
[Example CL](https://chromium-review.googlesource.com/c/chromium/src/+/6832181)

1. Define the event and observer method

Define a struct for the event payload and add it to the Event union.

`//chrome/browser/ui/tabs/tab_strip_api/tab_strip_api_events.mojom`

```
struct OnTabReloaded {
  NodeId tab_id;
  bool is_reloaded;
};

union Event {
  // ...
  OnTabReloadedEvent on_tab_reloaded;
};
```

`//chrome/browser/ui/tabs/tab_strip_api/tab_strip_api.mojom`

```
union TabsEvent {
  // ...
  OnTabReloaded(OnTabReloadedEvent event);
};
```

2. Translate the browser event

Update the `TabStripEventRecorder` to listen for the corresponding notification,
convert it to the mojo struct, and broadcast it.

`//chrome/browser/ui/tabs/tab_strip_api/events/event_transformation.cc`

```
mojom::OnTabReloadedEventPtr ToEvent(TabStripModelAdapter* adapter, int index) {
  auto event = mojom::OnTabReloadedEvent::New();
  event->is_reloaded = adapter->IsTabReloaded(index);
  return event;
}
```

`//chrome/browser/ui/tabs/tab_strip_api/events/tab_strip_event_recorder.cc`

```
void TabStripEventRecorder::TabReloadedChangedEvent(
    int index) {
  Handle(ToEvent(tab_strip_model_adapter_, index));
}
```

3. Update the Event Broadcaster

Update `EventVisitor` to handle the new event type. This confirms the event is
dispatched to the right method.

`//chrome/browser/ui/tabs/tab_strip_api/event_broadcaster.cc`

```
class EventVisitor {
 public:
  // ...

  void operator()(const mojom::OnTabReloadedEventPtr& event) {
    (*target_)->OnTabReloaded(event.Clone());
  }
};
```

## Key Architectural Patterns

### Adapters
Classes in `//chrome/browser/ui/tabs/tab_strip_api/adapters` act as the
abstraction layer that decouples the tab strip api from the browser. This
adapter pattern is key to our testing strategy, allowing us to decouple nested
objects by using various substitutions without instantiating the entire browser.

### Data converters
Functions in `//chrome/browser/ui/tabs/tab_strip_api/converters` translate
between internal browser classes and public mojo structs. This layer acts as a
guard so the API contract can remain stable even if the internal browser
structure changes.

### Event Propagation
Events flow from the browser to the client can be summarized in the following
pipeline.

`TabStripModel Change` -> `TabStripEventRecorder` -> `Translate`
-> `TabStripServiceImpl` -> `EventBroadcaster` ->
`Notify all TabsObserver clients`

### Tree Building
The browser's tab strip internal state is a complex tree represented with
TabCollection. Sending raw pointers is not suitable for Mojo hence the structure
is represented entirely of simple structs and unions.

The `MojoTreeBuilder` resolves this by walking the TabCollection tree and
building a copy composed entirely of simple Mojo structs `mojom::Container`.
This builder acts as a translator, creating a read-only, snapshot of the tab
strip state. This is the mechanism used by GetTabs() to provide the client with
its initial view of the tab strip.
