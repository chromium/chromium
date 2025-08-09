# TabStripService API Guide
This document provides a guide for using the TabStripService API, a Mojo-based
interface for interacting with the browser's tab strip. This API allows clients
to query tab information, perform actions, and listen for events related to the
tab strip.

## Getting Started
#### Feature Flag
To use the TabStripService API, you must enable it via its feature flag.
Launch Chrome with the following command-line argument:

`out\Default\chrome --enable-features="TabStripBrowserApi"`

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

2. Fetch state and listen for Events
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