self.addEventListener("connect", (evt) => {
  evt.ports[0].postMessage("ready");
});
