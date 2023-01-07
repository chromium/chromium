self.addEventListener("connect", (evt) => {
  const port = evt.ports[0];

  port.addEventListener("message", (event) => {
    fetch(event.data)
      .then((response) => port.postMessage({ ok: response.ok }))
      .catch((error) => port.postMessage({ error: error.name }));
  });
  port.start();

  port.postMessage("ready");
});
