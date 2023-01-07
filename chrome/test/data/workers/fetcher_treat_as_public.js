self.addEventListener("message", (event) => {
  console.log("Fetching", event.data);
  fetch(event.data)
    .then((response) => self.postMessage({ ok: response.ok }))
    .catch((error) => self.postMessage({ error: error.name }));
});
