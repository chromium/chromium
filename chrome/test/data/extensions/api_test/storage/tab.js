chrome.test.runTests([function tab() {
  // Check that the localstorage stuff we stored is still there.
  chrome.test.assertTrue(localStorage.foo == "bar");
}]);
