# WebID Android Feature

This folder contains the Android UI implementation for the WebID feature.

[TOC]

## Use case

This component displays a set of accounts. The user selects one which is then
used to complete authentication via WebID mediation based approach.


## Folder Structure

#### java/

The root folder contains the public interface of this component and data that is
used to fill it with content, e.g. Accounts.

Add `chrome/browser/ui/android/webid/android:public_java` as dependency to use
the interface and classes defined here.

#### java/internal/

Contains the actual implementation. Don't try to use any class defined here
outside of this package. If you need access to any method, consider making it
part of the public interface as defined in `AccountSelectionComponent`

This folder contains a separate [README](internal/README.md) that explains in
detail how the architecture looks like and how to extend the component further.

#### junit/

Contains Robolectric tests that test the delegate of the component. These tests
run without emulator which means that they are incredibly fast but cannot
instantiate or verify Android Views. The public build and run target is
`chrome_junit_tests`. Run them with:

``` bash
./out/<OutDirectory>/bin/run_chrome_junit_tests -f*AccountSelection*
```


#### javatests/

Contains View and Integration tests. These instrumented unit tests need an
emulator or a physical device which means that they run slowly but can test
rendered Views. Native Calls should be used very sparingly to ensure access to
all methods in the Dex path list. The public build and run target is
`chrome_public_test_apk`. Run them with:

``` bash
./out/<OutDirectory>/bin/run_chrome_public_test_apk -f *AccountSelection*
```

## Example usage

``` java

// Currently, you need access to internal/ to instantiate the component:
AccountSelectionComponent component = new AccountSelectionCoordinator(/*...*/);

component.initialize(activity, activity.getBottomSheetController(), () -> {
  // Things to do when the component is dismissed.
}));

List<Account> accounts; // Add accounts to show!
component.showAccounts("www.displayed-url.example", accounts, (account) -> {
  // The |account| that was clicked should be used to fill something now.
});

```
