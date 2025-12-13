# TabStripService API Guide
This document provides a guide for using the TabStripService API, a Mojo-based
interface for interacting with the browser's tab strip. This API allows clients
to query tab information, perform actions, and listen for events related to the
tab strip.

#### Other documentation
- [Developer Guide](tab_strip_api_developer_guide.md)

## Getting Started
#### API Source
The core of the API, including all data structures and interfaces, is defined in
the following Mojo files:

```
//chrome/browser/ui/tabs/tab_strip_api/tab_strip_api.mojom
//chrome/browser/ui/tabs/tab_strip_api/tab_strip_api_data_model.mojom
//chrome/browser/ui/tabs/tab_strip_api/tab_strip_api_events.mojom
//chrome/browser/ui/tabs/tab_strip_api/tab_strip_api_types.mojom
```

For up-to-date usage examples, please refer to the browser tests located in
`chrome/browser/ui/tabs/tab_strip_api/tab_strip_service_impl_browsertest.cc`.
TODO(crbug.com/409086859): add link to demo.

## How to Use the API
1. Instantiate the service

To begin interacting with the API, you first need to get the TabStripService
instance from the BrowserWindowFeatures and bind a remote to it.

```
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_api.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

// ...
mojo::Remote<tabs_api::mojom::TabStripService> remote;
auto* tab_strip_service =
        browser_->browser_window_features()->tab_strip_service();

if (tab_strip_service) {
  tab_strip_service->Accept(remote.BindNewPipeAndPassReceiver());
}
```

2. Fetch state and listen for events

The GetTabs() method serves a dual purpose: it provides a complete snapshot of
the current tab strip hierarchy and returns a pending_associated_remote that you
can bind to an observer to receive event updates.

```
#include "mojo/public/cpp/bindings/associated_receiver.h"

class MyTabStripClient : public tabs_api::mojom::TabsObserver {
 public:
  // ...
  void StartObserving(tabs_api::mojom::TabStripService* remote) {
    // This call requests the initial tab state and the event stream.
    remote->GetTabs(base::BindOnce(&MyTabStripClient::OnTabsReceived,
                                   base::Unretained(this)));
  }

 private:
  // Callback for the initial GetTabs() request.
  void OnTabsReceived(
      base::expected<tabs_api::mojom::TabsSnapshotPtr,
                     mojo_base::mojom::ErrorPtr> result) {
    if (result.has_value()) {
      // Bind the receiver to start listening for events
      receiver_.Bind(std::move(result.value()->stream));

      // Process the UI
      BuildUI(*result.value()->tab_strip);
    }
  }

  // TabsObserver Overrides
  // ...

  mojo::AssociatedReceiver<tabs_api::mojom::TabsObserver> receiver_{this};
};
```

3. Traversing the Tab Collection Tree

The tab strip is represented as a tree of TabCollectionContainer and
TabContainer objects. A TabCollectionContainer can contain tabs or other
collections (like tab groups). You can traverse this tree recursively to build a
UI representation.

The initial tree structure is in the TabsSnapshot from the GetTabs() call.

```
void BuildUI(
    const tabs_api::mojom::TabCollectionContainer& container) {

  tab_strip_view_->CreateCollectionView(container.collection);

  // Iterate through all the children of the tree
  for (const auto& element : container.elements) {
    if (element->is_tab_container()) {
      const auto& tab = element->get_tab_container()->tab;
      tab_strip_view_->CreateTabView(tab, container.collection.id);
    } else if (element->is_tab_collection_container()) {
      // Recurse to process this container
      const auto& nested_container =
          element->get_tab_collection_container();
      BuildUI(nested_container);
    }
  }
}
```

## Mojo Tips and Tricks
Working with Mojo involves a few key concepts. Here are some tips and tricks for
working with Mojo.

### Build Process and Generated Files
Mojo `.mojom` files are IDL files. During the build process, there is a mojo
bindings generator that will create language-specific files.
- C++: tab_strip_api.mojom
    => `out/.../gen/.../tab_strip_api.mojom.h and tab_strip_api.mojom.cc`
- Web: tab_strip_api.mojom
    => `out/.../gen/.../tab_strip_api.mojom-webui.js`

You should never edit the generated files directly. All changes must be made in
the source `.mojom` file.

### Codesearch
Because of the generated files, searching for an API can be noisy.
To find the API definition file, use the `-f:out` filter in Codesearch.

Example: `tab_strip_api.mojom -f:out`.

### Unions
A Mojo union provides a way to handle data that can be several different types.
One example of a union is `tabs_api::mojom::Data`.

```
// mojom file
union Data {
  Tab tab;
  TabStrip tab_strip;
  PinnedTabs pinned_tabs;
  UnpinnedTabs unpinned_tabs;
  TabGroup tab_group;
  SplitTab split_tab;
};
```

In C++, a union is generated as a std::variant. You can check the active member
using `is_object()` and `get_object()`.

```
tabs_api::NodeId GetTab(tabs_api::mojom::Data* data) {
  tabs_api::NodeId id;
  if (data->is_tab()) {
    id = data->get_tab()->id();
  }
  return id;
}
```

In typescript, a union is an object with a single key to a member in camelCase.
You can check the key's existence to determine the type.

```
if (data.pinnedTabs) {
  id = data.pinnedTabs.id;
} else if (data.tabGroup) {
  id = data.tabGroup.id;
}
```

### Ownership
Because Mojo is designed for IPC, you cannot pass raw C++ pointers even for
intra-process communication. All data is serialized into a message, sent across
a boundary, then deserialized. The `Clone()` method is the explicit way to deep
copy a Mojo object. You use it when you need to pass objects around without
giving up ownership of the original.

### Typemapping
Typemapping tells the bindings generator to use an existing native class. This
avoids creating redundant data structs that need manual conversion. For
example, `mojom::NodeId` is typemapped to a C++ class `tabs_api::NodeId` without
having to manually convert anything.

Typemaps are configured in the BUILD.gn file. There are various structs already
typemapped in `//chrome/browser/ui/tabs/tab_strip_api/types`.